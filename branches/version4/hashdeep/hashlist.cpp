// $Id$ 

/** hashlist.cpp
 * Contains the logic for performing the audit.
 * Also has implementation of the hashlist, searching, etc.
 * Formerly this code was in audit.cpp and match.cpp.
 */

#include "main.h"
#include <new>
#include <iostream>

/**
 * Return the number of entries in the hashlist that have used==0
 * Optionally display them, optionally with additional output.
 */
uint64_t hashlist::compute_unused(bool display, std::string annotation)
{
    uint64_t count=0;
    for(std::vector<file_data_t *>::const_iterator i = this->begin(); i != this->end(); i++){
	if((*i)->matched_file_number==0){
	    count++;
	    if (display || opt_verbose >= MORE_VERBOSE) {
		display_filename(stdout,*i,false);
		print_status(annotation.c_str());
	    }
	}
    }
    return count;
}

void hashlist::hashmap::add_file(file_data_t *fi,int alg_num)
{
    if(fi->hash_hex[alg_num].size()){
	fi->retain();
	insert(std::pair<std::string,file_data_t *>(fi->hash_hex[alg_num],fi));
    }
}


/**
 * Adds a file_data_t pointer to the hashlist.
 * Does not copy the object; that must be done elsewhere.
 * Notice we add the hash whether it is in use or not, as long as we have it.
 */
void hashlist::add_fdt(file_data_t *fi)
{ 
    fi->retain();
    push_back(fi);
    for(int i=0;i<NUM_ALGORITHMS;i++){
	hashmaps[i].add_file(fi,i);
    };
}

/** 
 * search for a hash
 */
file_data_t * hashlist::find_hash(hashid_t alg,std::string &hash_hex,uint64_t file_number)
{
    std::map<std::string,file_data_t *>::iterator it = hashmaps[alg].find(hash_hex);
    if(it==hashmaps[alg].end()) return 0;
    (*it).second->matched_file_number = file_number;	// note that it's used!
    return (*it).second;
}


/**
 * Search for the provided fdt in the hashlist and return the status of the match.
 */
hashlist::searchstatus_t hashlist::search(const file_data_hasher_t *fdht,file_data_t **matched) const
{
    bool file_size_mismatch = false;
    bool file_name_mismatch = false;
    bool did_match = false;
  
    /* Iterate through each of the hashes in the haslist until we find a match.
     */
    for (int i = 0 ; i < NUM_ALGORITHMS ; ++i)  {
	/* Only search hash functions that are in use and hashes that are in the fdt */
	if (hashes[i].inuse && fdht->hash_hex[i].size()){
	    hashmap::const_iterator it = hashmaps[i].find(fdht->hash_hex[i]);
	    if(it != hashmaps[i].end()){
		/* found a match*/

		did_match = true;

		const file_data_t *match = it->second;
		if(matched){
		    (*matched)   = it->second; // make a copy
		    it->second->matched_file_number = fdht->file_number;
		}

		/* Verify that all of the other hash functions for *it match fdt as well. */
		for(int j=0;j<NUM_ALGORITHMS;j++){
		    if(hashes[j].inuse && j!=i
		       && fdht->hash_hex[i].size()
		       && match->hash_hex[i].size()){
			if(fdht->hash_hex[j] != match->hash_hex[j]){
			    /* Amazing. We found a match on one hash a a non-match on another.
			     * Call the newspapers! This is a newsorthy event.
			     */
			    return status_partial_match;
			}
		    }
		}
		/* If we got here we matched on all of the hashes.
		 * Which is to be expected.
		 * Check to see if the sizes are the same.
		 */
		if(fdht->file_size != match->file_size){
		    /* Amazing. We found two files that have the same hash but different
		     * file sizes. This has never happened before in the history of the world.
		     * Call the newspapers!
		     */
		    file_size_mismatch = true;
		}
		/* See if the hashes are the same but the name changed.
		 */
		if(fdht->file_name != match->file_name){
		    file_name_mismatch = true;
		}
	    }
	}
    }
    if(did_match==false) return status_no_match;

    /* If we get here, then all of the hash matches for all of the algorithms have been
     * checked and found to be equal if present.
     */
    if(file_size_mismatch) return status_file_size_mismatch;
    if(file_name_mismatch) return status_file_name_mismatch;
    return status_match;
}



/**
 * Returns the file type of a given input file.
 * fn is provided so that error messages can be printed.
 */
hashlist::filetype_t hashlist::identify_filetype(const char *fn,FILE *handle)
{
    char buf[MAX_STRING_LENGTH];

    // Find the header 
    if ((fgets(buf,MAX_STRING_LENGTH,handle)) == NULL) {
	return file_unknown;
    }

    chop_line(buf);

    if ( ! STRINGS_EQUAL(buf,HASHDEEP_HEADER_10)) {
	return file_unknown;
    }

    // Find which hashes are in this file
    if ((fgets(buf,MAX_STRING_LENGTH,handle)) == NULL) {
	return file_unknown;
    }

    chop_line(buf);

    // We don't use STRINGS_EQUAL here because we only care about
    // the first ten characters for right now. 
    if (strncasecmp("%%%% size,",buf,10))  {
	return file_unknown;
    }

    /**
     * Remember previously loaded hashes.
     */
    std::string previously_enabled_algorithms = last_enabled_algorithms; 
    
    // Skip the "%%%% size," when parsing the list of hashes 
    enable_hashing_algorithms_from_hashdeep_file(fn,buf + 10);
    
    // If the set of hashes now in use doesn't match those previously in use,
    // give a warning.
    if (previously_enabled_algorithms.size()>0 && previously_enabled_algorithms != last_enabled_algorithms){
	print_error("%s: %s: Hashes not in same format as previously loaded",
		    __progname, fn);
    }
    return file_hashdeep_10;
}


/*
 * Examine the list of hashing algorithms in the file,
 * enable them and note their order. If the last algorithm is 'filename', ignore it.
 */

void hashlist::enable_hashing_algorithms_from_hashdeep_file(std::string fn,std::string val)
{
    // The first position is always the file size, so we start with an 
    // the first position of one.
    uint8_t num_columns = 1;		
  
    last_enabled_algorithms = val;
    std::vector<std::string> algs = split(val,',');
    for(std::vector<std::string>::iterator it = algs.begin(); it!=algs.end(); it++){
	std::string name = *it;
	lowercase(name);
	if(name=="filename") continue;
	hashid_t id = algorithm_t::get_hashid_for_name(name);
	if(id==alg_unknown){
	    print_error("%s: %s: Badly formatted file", __progname, fn.c_str());
	    try_msg();
	    exit(EXIT_FAILURE);
	}
	    
	/* Found a known algorithm */
	hashes[id].inuse = TRUE;
	hash_column[num_columns] = id;
	num_columns++;
    }
}


void hashlist::dump_hashlist()
{
    for(hashlist::const_iterator it = begin(); it!=end(); it++){
	std::cout << (*it)->file_size << "," << (*it)->hash_hex << "," << (*it)->file_name << "\n";
    }
}


/**
 * Loads a file of known hashes.
 * First identifies the file type, then reads the file.
 */
hashlist::loadstatus_t hashlist::load_hash_file(const char *fn)
{
    loadstatus_t status = loadstatus_ok;
    filetype_t type;

    FILE *handle = fopen(fn,"rb");
    if (NULL == handle) {
	print_error("%s: %s: %s", __progname, fn, strerror(errno));
	return status_file_error;
    }
  
    type = identify_filetype(fn,handle);
    if (file_unknown == type)  {
	print_error("%s: %s: Unable to identify file format", __progname, fn);
	fclose(handle);
	return status_unknown_filetype;
    }

    int contains_bad_lines = FALSE;
    int record_valid=0;

    // We start our counter at line number two for the two lines
    // of header we've already read
    uint64_t line_number = 2;

    /* Redo this to use std::string everywhere */
    char line[MAX_STRING_LENGTH];	// holds the line we are reading

    while (fgets(line,MAX_STRING_LENGTH,handle)) {
	line_number++;			// count where we are

	// Lines starting with a pound sign are comments and can be ignored
	if ('#' == line[0]){
	    continue;
	}

	// C++ typically fails with a bad_alloc, but you can make it return null
	// http://www.cplusplus.com/reference/std/new/bad_alloc/
	// http://www.cplusplus.com/reference/std/new/nothrow/
	file_data_t *t = new (std::nothrow) file_data_t(); // C++ new fails with a bad_a
	if (NULL == t){
	    fatal_error("%s: %s: Out of memory in line %"PRIu64, 
			__progname, fn, line_number);
	}

	chop_line(line);
	record_valid = TRUE;

	// completely rewritten to use STL strings
	std::vector<std::string> fields = split(std::string(line),',');
	for(size_t column_number=0;column_number<fields.size();column_number++){
	    // The first column should always be the file size
	    std::string word = fields[column_number];
	    if (column_number==0) {
		t->file_size = (uint64_t)strtoll(word.c_str(),NULL,10);
		continue;
	    }
	    if (column_number==fields.size()-1){
		t->file_name = word;
		continue;
	    }


	    // All other columns should contain a valid hash in hex
	    if ( !algorithm_t::valid_hash(hash_column[column_number],word)){
		print_error("%s: %s: Invalid %s hash in line %"PRIu64,__progname, fn, 
			    hashes[hash_column[column_number]].name.c_str(),
			    line_number);
		contains_bad_lines = TRUE;
		record_valid = FALSE;
		// Break out (done = true) and then process the next line
		break;
	    }

	    // Convert the hash to a std::string and save it
	    lowercase(word);
	    t->hash_hex[hash_column[column_number]] = word;
	}
	if ( record_valid) {
	    add_fdt(t);	/* add the file to the database*/
	}
    }
    fclose(handle);

    if (contains_bad_lines){
	return status_contains_bad_hashes;
    }
    
    return status;
}


 /**
  * We don't use this function anymore, but it's handy to have just in case
  */
const char *hashlist::searchstatus_to_str(searchstatus_t val)
{
    switch (val) {
    case searchstatus_ok: return "ok";
    case status_match:    return "complete match";
    case status_partial_match: return "partial match";
    case status_file_size_mismatch: return "file size mismatch";
    case status_file_name_mismatch: return "file name mismatch";
    case status_no_match: return "no match"; 
	
    default:
	return "unknown";
    }      
}



