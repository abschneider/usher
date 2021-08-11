#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <boost/program_options.hpp> 
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <iostream>
#include <memory>
#include <limits>
#include "boost/filesystem.hpp"
#include "usher_graph.hpp"
#include "parsimony.pb.h"
#include "version.hpp"
#include "usher_common.hpp"
namespace po = boost::program_options;
namespace MAT = Mutation_Annotated_Tree;



int main(int argc, char** argv) {

    //Variables to load command-line options using Boost program_options
    std::string arg_filename;
    uint32_t num_cores = tbb::task_scheduler_init::default_num_threads();
    po::options_description desc{"Options"};
    uint32_t sleep_length;

    desc.add_options()
        //("list-mutation-annonated-trees,i", po::value<std::string>(&din_filename)->default_value(""), "List of mutation-annotated tree objects");
        //this will be used if multiple MATs were to be stored in future
        ("arguments,a", po::value<std::string>(&arg_filename)->required(), "Input argument file that will contain arguments for usher [REQUIRED]")
        ("sleep-length,s", po::value<uint32_t>(&sleep_length)->default_value(100), "Time in milliseconds that the program waits until checking for input in argument file")
        ("help,h", "Print help messages");

    
    po::options_description all_options;
    all_options.add(desc);

    po::variables_map vm;
    try{
        po::store(po::command_line_parser(argc, argv).options(all_options).run(), vm);
        po::notify(vm);
    }
    catch(std::exception &e){
        //return with error code 1 unless the user specifies help
        std::cerr << desc << std::endl;
        if(vm.count("help"))
            return 0;
        else
            return 1;
    }

    
    MAT::Tree loaded_MAT;
    std::string loaded_MAT_name = "";
    if(!boost::filesystem::exists(arg_filename)){
        std::cout << "Arguments file not found" <<std::endl;
        return 1;
    }
    boost::filesystem::path p = boost::filesystem::current_path();
    std::time_t modified_time(0);
    MAT::Tree curr_tree; //MAT that is used for each time copy of MAT is needed
    bool curr_tree_avail = false;//keep track if curr_tree is a new copy of MAT and can be used

    // timer object to be used to measure runtimes of individual stages
    Timer timer; 

    while(true){
        if(loaded_MAT_name != ""){
            timer.Start();
            fprintf(stderr, "Copying pre-loaded mutation-annotated tree object originally from file %s\n", loaded_MAT_name.c_str());
            curr_tree = MAT::get_tree_copy(loaded_MAT);
            fprintf(stderr, "Completed in %ld msec \n\n", timer.Stop());
            curr_tree_avail = true;
        }
        while(boost::filesystem::last_write_time(p) == modified_time){
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_length));
        }
        
        modified_time = boost::filesystem::last_write_time(p);
        std::ifstream arguments_file(arg_filename);
        if (!arguments_file) {
            fprintf(stderr, "ERROR: Could not open the arguments file: %s!\n", arg_filename.c_str());
            exit(1);
        }
        std::string argument;
        
        

        //get a line of argument and feed it into usher
        while(std::getline(arguments_file, argument)){
            fprintf(stderr, "Argument: %s \n\n", argument.c_str());
            std::istringstream arg(argument);
            std::vector<std::string> arg_vector; //to store each word from arg
            arg_vector.emplace_back("./usher"); //to replicate commandline argument
            std::string tempStr; //hold the word from argument before adding to the vector
            while(arg >> tempStr){
                arg_vector.emplace_back(tempStr);
            }
            
            int argc_line = arg_vector.size(); 
            const char* argv_line[argc_line];
            for(int i = 0; i < argc_line; i++){
                argv_line[i] = arg_vector[i].c_str();
            }

            /**
            *  read inputs
            *
            * Commands not allowed:
            *   - "tree,t"
            *   - "multiple-placements,M"
            */
            //Variables to load command-line options using Boost program_options
            std::string din_filename;
            std::string dout_filename;
            std::string outdir;
            std::string vcf_filename;
            uint32_t num_threads;
            uint32_t max_trees = 1; //only one tree for usher_server
            uint32_t max_uncertainty;
            bool sort_before_placement_1 = false;
            bool sort_before_placement_2 = false;
            bool sort_before_placement_3 = false;
            bool reverse_sort = false;
            bool collapse_tree=false;
            bool collapse_output_tree=false;
            bool print_uncondensed_tree = false;
            bool print_parsimony_scores = false;
            bool retain_original_branch_len = false;
            bool no_add = false;
            bool detailed_clades = false;
            size_t print_subtrees_size=0;
            size_t print_subtrees_single=0;    
            po::options_description desc{"Options"};

            std::string num_threads_message = "Number of threads to use when possible [DEFAULT uses all available cores, " + std::to_string(num_cores) + " detected on this machine]";
            desc.add_options()
                ("vcf,v", po::value<std::string>(&vcf_filename)->required(), "Input VCF file (in uncompressed or gzip-compressed .gz format) [REQUIRED]")
                ("outdir,d", po::value<std::string>(&outdir)->default_value("."), "Output directory to dump output and log files [DEFAULT uses current directory]")
                ("load-mutation-annotated-tree,i", po::value<std::string>(&din_filename)->default_value(""), "Load mutation-annotated tree object")
                ("save-mutation-annotated-tree,o", po::value<std::string>(&dout_filename)->default_value(""), "Save output mutation-annotated tree object to the specified filename")
                ("sort-before-placement-1,s", po::bool_switch(&sort_before_placement_1), \
                 "Sort new samples based on computed parsimony score and then number of optimal placements before the actual placement [EXPERIMENTAL].")
                ("sort-before-placement-2,S", po::bool_switch(&sort_before_placement_2), \
                 "Sort new samples based on the number of optimal placements and then the parsimony score before the actual placement [EXPERIMENTAL].")
                ("sort-before-placement-3,A", po::bool_switch(&sort_before_placement_3), \
                 "Sort new samples based on the number of ambiguous bases [EXPERIMENTAL].")
                ("reverse-sort,r", po::bool_switch(&reverse_sort), \
                 "Reverse the sorting order of sorting options (sort-before-placement-1 or sort-before-placement-2) [EXPERIMENTAL]")
                ("collapse-tree,c", po::bool_switch(&collapse_tree), \
                 "Collapse internal nodes of the input tree with no mutations and condense identical sequences in polytomies into a single node and the save the tree to file condensed-tree.nh in outdir")
                ("collapse-output-tree,C", po::bool_switch(&collapse_output_tree), \
                 "Collapse internal nodes of the output tree with no mutations before the saving the tree to file final-tree.nh in outdir")
                ("max-uncertainty-per-sample,e", po::value<uint32_t>(&max_uncertainty)->default_value(1e6), \
                 "Maximum number of equally parsimonious placements allowed per sample beyond which the sample is ignored")
                ("write-uncondensed-final-tree,u", po::bool_switch(&print_uncondensed_tree), "Write the final tree in uncondensed format and save to file uncondensed-final-tree.nh in outdir")
                ("write-subtrees-size,k", po::value<size_t>(&print_subtrees_size)->default_value(0), \
                 "Write minimum set of subtrees covering the newly added samples of size equal to this value")
                ("write-single-subtree,K", po::value<size_t>(&print_subtrees_single)->default_value(0), \
                 "Similar to write-subtrees-size but produces a single subtree with all newly added samples along with random samples up to the value specified by this argument")
                ("write-parsimony-scores-per-node,p", po::bool_switch(&print_parsimony_scores), \
                 "Write the parsimony scores for adding new samples at each existing node in the tree without modifying the tree in a file names parsimony-scores.tsv in outdir")
                ("retain-input-branch-lengths,l", po::bool_switch(&retain_original_branch_len), \
                 "Retain the branch lengths from the input tree in out newick files instead of using number of mutations for the branch lengths.")
                ("no-add,n", po::bool_switch(&no_add), \
                 "Do not add new samples to the tree")
                ("detailed-clades,D", po::bool_switch(&detailed_clades), \
                 "In clades.txt, write a histogram of annotated clades and counts across all equally parsimonious placements")
                ("threads,T", po::value<uint32_t>(&num_threads)->default_value(num_cores), num_threads_message.c_str())
                ("version", "Print version number")
                ("help,h", "Print help messages");
    
            po::options_description all_options;
            all_options.add(desc);

            po::variables_map vm;
            try{
                po::store(po::command_line_parser(argc_line, argv_line).options(all_options).run(), vm);
                po::notify(vm);
            }
            catch(std::exception &e){
                if (vm.count("version")) {
                    std::cout << "UShER (v" << PROJECT_VERSION << ")" << std::endl;
                }
                else {
                    std::cerr << "UShER (v" << PROJECT_VERSION << ")" << std::endl;
                    std::cerr << desc << std::endl;
                }
                // Return with error code 1 unless the user specifies help or version
                if(vm.count("help") || vm.count("version"))
                    continue;//if help or version then go to next line
                else
                    break;//if error encountered then stop reading the file for now
            }
            //compare MAT and if same copy it
            

            if(din_filename != loaded_MAT_name){

                timer.Start();
                fprintf(stderr, "Loading existing mutation-annotated tree object from file %s\n", din_filename.c_str());
       
                if(loaded_MAT_name != ""){ //if there is an existing trees, delete them 
                    MAT::clear_tree(loaded_MAT);
                }
                if(curr_tree_avail){
                    MAT::clear_tree(curr_tree);
                }
                // Load mutation-annotated tree and store it
                loaded_MAT = MAT::load_mutation_annotated_tree(din_filename);
                loaded_MAT_name = din_filename;
                fprintf(stderr, "Completed in %ld msec \n\n", timer.Stop());
                fprintf(stderr, "Copying the mutation-annotated tree object\n");
                curr_tree = MAT::get_tree_copy(loaded_MAT);
                fprintf(stderr, "Completed in %ld msec \n\n", timer.Stop());
                
            }else if(!curr_tree_avail){
                timer.Start();
                fprintf(stderr, "Copying pre-loaded mutation-annotated tree object originally from file %s\n", din_filename.c_str());
                    curr_tree = MAT::get_tree_copy(loaded_MAT);
                fprintf(stderr, "Completed in %ld msec \n\n", timer.Stop());
            }

            
            
            // Variables below used to store the different fields of the input VCF file 
            bool header_found = false;
            std::vector<std::string> variant_ids;
            std::vector<Missing_Sample> missing_samples;

            // Vector used to store all tree nodes in breadth-first search (BFS) order
            std::vector<MAT::Node*> bfs;
            // Map the node identifier string to index in the BFS traversal
            std::unordered_map<std::string, size_t> bfs_idx;
    
            // Vectore to store the names of samples which have a high number of 
            // parsimony-optimal placements
            std::vector<std::string> low_confidence_samples;
                    fprintf(stderr, "Loading VCF file\n");
            timer.Start();

            // Boost library used to stream the contents of the input VCF file in
            // uncompressed or compressed .gz format
            std::ifstream infile(vcf_filename, std::ios_base::in | std::ios_base::binary);
            if (!infile) {
                fprintf(stderr, "ERROR: Could not open the VCF file: %s!\n", vcf_filename.c_str());
                exit(1);
            }
            boost::iostreams::filtering_istream instream;
            try {
                if (vcf_filename.find(".gz\0") != std::string::npos) {
                    instream.push(boost::iostreams::gzip_decompressor());
                }
                instream.push(infile);
            }
            catch(const boost::iostreams::gzip_error& e) {
                std::cout << e.what() << '\n';
            }

            std::vector<size_t> missing_idx;
            std::string s;
            // This while loop reads the VCF file line by line and populates
            // missing_samples and missing_sample_mutations based on the names and 
            // variants of missing samples. If a sample name in the VCF is already
            // found in the tree, it gets ignored with a warning message
            while (instream.peek() != EOF) {
                std::getline(instream, s);
                std::vector<std::string> words;
                MAT::string_split(s, words);
                if ((not header_found) && (words.size() > 1)) {
                    if (words[1] == "POS") {
                        for (size_t j=9; j < words.size(); j++) {
                            variant_ids.emplace_back(words[j]);
                            if ((curr_tree.get_node(words[j]) == NULL) && (curr_tree.condensed_leaves.find(words[j]) == curr_tree.condensed_leaves.end())) {
                                missing_samples.emplace_back(Missing_Sample(words[j]));
                                missing_idx.emplace_back(j);
                            }
                            else {
                                fprintf(stderr, "WARNING: Ignoring sample %s as it is already in the tree.\n", words[j].c_str());
                            }
                        }
                        header_found = true;
                    }
                }
                else if (header_found) {
                    if (words.size() != 9+variant_ids.size()) {
                        fprintf(stderr, "ERROR! Incorrect VCF format. Expected %zu columns but got %zu.\n", 9+variant_ids.size(), words.size());
                        exit(1);
                    }
                    std::vector<std::string> alleles;
                    alleles.clear();
                    MAT::string_split(words[4], ',', alleles);
                    for (size_t k = 0; k < missing_idx.size(); k++) {
                        size_t j = missing_idx[k];
                        auto iter = missing_samples.begin();
                        std::advance(iter, k);
                        if (iter != missing_samples.end()) {
                            MAT::Mutation m;
                            m.chrom = words[0];
                            m.position = std::stoi(words[1]);
                            m.ref_nuc = MAT::get_nuc_id(words[3][0]);
                            assert((m.ref_nuc & (m.ref_nuc-1)) == 0); //check if it is power of 2
                            m.par_nuc = m.ref_nuc;
                            // Alleles such as '.' should be treated as missing
                            // data. if the word is numeric, it is an index to one
                            // of the alleles 
                            if (isdigit(words[j][0])) {
                                int allele_id = std::stoi(words[j]);
                                if (allele_id > 0) { 
                                    std::string allele = alleles[allele_id-1];
                                    if (allele[0] == 'N') {
                                        m.is_missing = true;
                                        m.mut_nuc = MAT::get_nuc_id('N');
                                    }
                                    else {
                                        auto nuc = MAT::get_nuc_id(allele[0]);
                                        if (nuc == MAT::get_nuc_id('N')) {
                                            m.is_missing = true;
                                        }
                                        else {
                                            m.is_missing = false;
                                        }
                                        m.mut_nuc = nuc;
                                    }
                                    (*iter).mutations.emplace_back(m);
                                }
                            }
                            else {
                                m.is_missing = true;
                                m.mut_nuc = MAT::get_nuc_id('N');
                                (*iter).mutations.emplace_back(m);
                            }
                            if ((m.mut_nuc & (m.mut_nuc-1)) !=0) {
                                (*iter).num_ambiguous++;
                            }
                        }
                    }
                }
            }
            curr_tree_avail = false;
            fprintf(stderr, "Completed in %ld msec \n\n", timer.Stop());
            std::vector<size_t> tree_parsimony_scores;
            int return_val = usher_common(dout_filename, outdir, num_threads, max_trees, max_uncertainty, 
            sort_before_placement_1, sort_before_placement_2, sort_before_placement_3, reverse_sort, collapse_tree, 
            collapse_output_tree, print_uncondensed_tree, print_parsimony_scores, retain_original_branch_len, no_add, 
            detailed_clades, print_subtrees_size, print_subtrees_single, missing_samples, low_confidence_samples, &curr_tree);
            MAT::clear_tree(curr_tree);
            if(return_val != 0){
                break;//if error encountered then stop reading the file for now
            }
        }
        arguments_file.close();
    }
}