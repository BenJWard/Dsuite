//
//  D.cpp
//  Dsuite
//
//  Created by Milan Malinsky on 11/04/2019.
//  Copyright © 2019 Milan Malinsky. All rights reserved.
//

#include "D.h"
#include <deque>
#define SUBPROGRAM "Dinvestigate"

#define DEBUG 1

static const char *ABBA_USAGE_MESSAGE =
"Usage: " PROGRAM_BIN " " SUBPROGRAM " [OPTIONS] INPUT_FILE.vcf.gz SETS.txt test_trios.txt\n"
"Calculate the admixture proportion estimates f_G, f_d (Martin et al. 2014 MBE), and f_dM (Malinsky et al., 2015)\n"
"Also outputs f_d and f_dM in genomic windows\n"
"The SETS.txt file should have two columns: SAMPLE_ID    POPULATION_ID\n"
"The test_trios.txt should contain names of three populations for which the statistics will be calculated:\n"
"POP1   POP2    POP3\n"
"There can be multiple lines and then the program generates multiple ouput files, named like POP1_POP2_POP3_localFstats_SIZE_STEP.txt\n"
"\n"
"       -h, --help                              display this help and exit\n"
"       -w SIZE,STEP --window=SIZE,STEP         (required) D, f_D, and f_dM statistics for windows containing SIZE useable SNPs, moving by STEP (default: 50,25)\n"
//"       --fJackKnife=WINDOW                     (optional) Calculate jackknife for the f_G statistic from Green et al. Also outputs \n"
"       -n, --run-name                          run-name will be included in the output file name\n"
"\n"
"\nReport bugs to " PACKAGE_BUGREPORT "\n\n";


//enum { OPT_F_JK };

static const char* shortopts = "hw:n:";

//static const int JK_WINDOW = 5000;

static const struct option longopts[] = {
    { "run-name",   required_argument, NULL, 'n' },
    { "window",   required_argument, NULL, 'w' },
    { "help",   no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 }
};

namespace opt
{
    static string vcfFile;
    static string setsFile;
    static string testTriosFile;
    static string runName = "";
    static int minScLength = 0;
    static int windowSize = 50;
    static int windowStep = 25;
    //int jkWindowSize = JK_WINDOW;
}


void doAbbaBaba() {
    string line; // for reading the input files
    
    std::istream* vcfFile = createReader(opt::vcfFile);
    std::ifstream* setsFile = new std::ifstream(opt::setsFile.c_str());
    std::ifstream* testTriosFile = new std::ifstream(opt::testTriosFile.c_str());
    
    // Get the sample sets
    std::map<string, std::vector<string>> speciesToIDsMap;
    std::map<string, string> IDsToSpeciesMap;
    std::map<string, std::vector<size_t>> speciesToPosMap;
    std::map<size_t, string> posToSpeciesMap;
    
    // Get the sample sets
    bool outgroupSpecified = false;
    int l = 0;
    while (getline(*setsFile, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end()); // Deal with any left over \r from files prepared on Windows
        l++; if (line == "") { std::cerr << "Please fix the format of the " << opt::setsFile << " file.\nLine " << l << " is empty." << std::endl; exit(EXIT_FAILURE); }
        // std::cerr << line << std::endl;
        std::vector<string> ID_Species = split(line, '\t');
        if (ID_Species.size() != 2) { std::cerr << "Please fix the format of the " << opt::setsFile << " file.\nLine " << l << " does not have two columns separated by a tab." << std::endl; exit(EXIT_FAILURE); }
        if (ID_Species[1] == "Outgroup") { outgroupSpecified = true; }
        speciesToIDsMap[ID_Species[1]].push_back(ID_Species[0]);
        IDsToSpeciesMap[ID_Species[0]] = ID_Species[1];
        //std::cerr << ID_Species[1] << "\t" << ID_Species[0] << std::endl;
    }
    if (!outgroupSpecified) { std::cerr << "The file " << opt::setsFile << " needs to specify the \"Outgroup\"" << std::endl; exit(1); }
    // Get a vector of set names (usually species)
    std::vector<string> species;
    for(std::map<string,std::vector<string>>::iterator it = speciesToIDsMap.begin(); it != speciesToIDsMap.end(); ++it) {
        if ((it->first) != "Outgroup") {
            species.push_back(it->first);
            // std::cerr << it->first << std::endl;
        }
    } std::cerr << "There are " << species.size() << " sets (excluding the Outgroup)" << std::endl;
    
    
    // Get the test trios
    std::vector<std::ofstream*> outFiles;
    std::vector<std::ofstream*> outFilesGenes;
    std::vector<std::vector<string> > testTrios;
    while (getline(*testTriosFile,line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end()); // Deal with any left over \r from files prepared on Windows
        // std::cerr << line << std::endl;
        std::vector<string> threePops = split(line, '\t'); assert(threePops.size() == 3);
        for (int i = 0; i != threePops.size(); i++) { // Check that the test trios are in the sets file
            if (speciesToIDsMap.count(threePops[i]) == 0) {
                std::cerr << threePops[i] << " is present in the " << opt::testTriosFile << " but missing from the " << opt::setsFile << std::endl;
            }
        }
        std::ofstream* outFile = new std::ofstream(threePops[0] + "_" + threePops[1] + "_" + threePops[2]+ "_localFstats_" + opt::runName + "_" + numToString(opt::windowSize) + "_" + numToString(opt::windowStep) + ".txt");
        *outFile << "chr\twindowStart\twindowEnd\tD\tf_d\tf_dM" << std::endl;
        outFiles.push_back(outFile);
        testTrios.push_back(threePops);
    }
    
    // And need to prepare the vectors to hold the PBS values and the coordinates:
    std::deque<double> initDeq(opt::windowSize,0.0); // deque to initialise per-site values
    // vector of three per-site PBS deques - for (ABBA-BABA), and the Fg, Fd, and Fdm denominators
    // and the final is for the coordinates
    std::vector<std::deque<double>> initFiveDeques(5,initDeq);
    std::vector<std::vector<std::deque<double>>> testTrioResults(testTrios.size(),initFiveDeques);
    
    // Now go through the vcf and calculate D
    int totalVariantNumber = 0;
    std::vector<int> usedVars(testTrios.size(),0); // Will count the number of used variants for each trio
    std::vector<int> usedVars_f_G(testTrios.size(),0); // Will count the number of used variants for each trio
    int reportProgressEvery = 1000; string chr; string coord;
    std::vector<double> ABBAtotals(testTrios.size(),0); std::vector<double> BABAtotals(testTrios.size(),0);
    std::vector<double> Genome_f_G_num(testTrios.size(),0); std::vector<double> Genome_f_G_denom(testTrios.size(),0);
    std::vector<double> Genome_f_D_denom(testTrios.size(),0); std::vector<double> Genome_f_DM_denom(testTrios.size(),0);
   // ABBA_BABA_Freq_allResults r;
   // int lastPrint = 0; int lastWindowVariant = 0;
   // std::vector<double> regionDs; std::vector<double> region_f_Gs; std::vector<double> region_f_Ds; std::vector<double> region_f_DMs;
    std::vector<string> sampleNames; std::vector<std::string> fields;
    clock_t start; clock_t startGettingCounts; clock_t startCalculation;
    double durationOverall; double durationGettingCounts; double durationCalculation;
    while (getline(*vcfFile, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end()); // Deal with any left over \r from files prepared on Windows
        if (line[0] == '#' && line[1] == '#')
            continue;
        else if (line[0] == '#' && line[1] == 'C') {
            fields = split(line, '\t');
            std::vector<std::string> sampleNames(fields.begin()+NUM_NON_GENOTYPE_COLUMNS,fields.end());
            // print_vector_stream(sampleNames, std::cerr);
            for (std::vector<std::string>::size_type i = 0; i != sampleNames.size(); i++) {
                posToSpeciesMap[i] = IDsToSpeciesMap[sampleNames[i]];
            }
            // Iterate over all the keys in the map to find the samples in the VCF:
            // Give an error if no sample is found for a species:
            for(std::map<string, std::vector<string>>::iterator it = speciesToIDsMap.begin(); it != speciesToIDsMap.end(); ++it) {
                string sp =  it->first;
                //std::cerr << "sp " << sp << std::endl;
                std::vector<string> IDs = it->second;
                std::vector<size_t> spPos = locateSet(sampleNames, IDs);
                if (spPos.empty()) {
                    std::cerr << "Did not find any samples in the VCF for \"" << sp << "\"" << std::endl;
                    assert(!spPos.empty());
                }
                speciesToPosMap[sp] = spPos;
            }
            start = clock();
        } else {
            totalVariantNumber++;
            if (totalVariantNumber % reportProgressEvery == 0) {
                durationOverall = ( clock() - start ) / (double) CLOCKS_PER_SEC;
                std::cerr << "Processed " << totalVariantNumber << " variants in " << durationOverall << "secs" << std::endl;
                std::cerr << "GettingCounts " << durationGettingCounts << " calculation " << durationCalculation << "secs" << std::endl;
            }
            fields = split(line, '\t'); chr = fields[0]; coord = fields[1];
            std::vector<std::string> genotypes(fields.begin()+NUM_NON_GENOTYPE_COLUMNS,fields.end());
            // Only consider biallelic SNPs
            string refAllele = fields[3]; string altAllele = fields[4];
            if (refAllele.length() > 1 || altAllele.length() > 1 || altAllele == "*") {
                refAllele.clear(); refAllele.shrink_to_fit(); altAllele.clear(); altAllele.shrink_to_fit();
                genotypes.clear(); genotypes.shrink_to_fit(); continue;
            }
            
            startGettingCounts = clock();
            GeneralSetCountsWithSplits* c = new GeneralSetCountsWithSplits(speciesToPosMap, (int)genotypes.size());
            c->getSplitCounts(genotypes, posToSpeciesMap);
            genotypes.clear(); genotypes.shrink_to_fit();
            durationGettingCounts = ( clock() - startGettingCounts ) / (double) CLOCKS_PER_SEC;
            
            startCalculation = clock();
            double p_O = c->setDAFs.at("Outgroup");
            if (p_O == -1) { delete c; continue; } // We need to make sure that the outgroup is defined
            
            double p_S1; double p_S2; double p_S3; double ABBA; double BABA; double F_d_denom; double F_dM_denom;
            for (int i = 0; i != testTrios.size(); i++) {
                try { p_S1 = c->setDAFs.at(testTrios[i][0]); } catch (const std::out_of_range& oor) {
                std::cerr << "Counts don't contain derived allele frequency for " << testTrios[i][0] << std::endl; }
                if (p_S1 == -1) continue;  // If any member of the trio has entirely missing data, just move on to the next trio
                try { p_S2 = c->setDAFs.at(testTrios[i][1]); } catch (const std::out_of_range& oor) {
                    std::cerr << "Counts don't contain derived allele frequency for " << testTrios[i][1] << std::endl; }
                if (p_S2 == -1) continue;
                try { p_S3 = c->setDAFs.at(testTrios[i][2]); } catch (const std::out_of_range& oor) {
                    std::cerr << "Counts don't contain derived allele frequency for " << testTrios[i][0] << std::endl; }
                if (p_S3 == -1) continue;
                usedVars[i]++;
                
                ABBA = ((1-p_S1)*p_S2*p_S3*(1-p_O)); ABBAtotals[i] += ABBA;
                BABA = (p_S1*(1-p_S2)*p_S3*(1-p_O)); BABAtotals[i] += BABA;
                if (p_S2 > p_S3) {
                    F_d_denom = ((1-p_S1)*p_S2*p_S2*(1-p_O)) - (p_S1*(1-p_S2)*p_S2*(1-p_O));
                } else {
                    F_d_denom = ((1-p_S1)*p_S3*p_S3*(1-p_O)) - (p_S1*(1-p_S3)*p_S3*(1-p_O));
                }
                Genome_f_D_denom[i] += F_d_denom;
                if (p_S1 <= p_S2) {
                    if (p_S2 > p_S3) {
                        F_dM_denom = ((1-p_S1)*p_S2*p_S2*(1-p_O)) - (p_S1*(1-p_S2)*p_S2*(1-p_O));
                    } else {
                        F_dM_denom = ((1-p_S1)*p_S3*p_S3*(1-p_O)) - (p_S1*(1-p_S3)*p_S3*(1-p_O));
                    }
                } else {
                    if (p_S1 > p_S3) {
                        F_dM_denom = -(((1-p_S1)*p_S2*p_S1*(1-p_O)) - (p_S1*(1-p_S2)*p_S1)*(1-p_O));
                    } else {
                        F_dM_denom = -(((1-p_S3)*p_S2*p_S3*(1-p_O)) - (p_S3*(1-p_S2)*p_S3)*(1-p_O));
                    }
                } Genome_f_DM_denom[i] += F_dM_denom;
                
                if (c->setAlleleCountsSplit1.at(testTrios[i][2]) > 0 && c->setAlleleCountsSplit2.at(testTrios[i][2]) > 0) {
                    double p_S3a = c->setAAFsplit1.at(testTrios[i][2]); double p_S3b = c->setAAFsplit2.at(testTrios[i][2]);
                    Genome_f_G_num[i] += ABBA - BABA;
                    Genome_f_G_denom[i] += ((1-p_S1)*p_S3a*p_S3b*(1-p_O)) - (p_S1*(1-p_S3a)*p_S3b*(1-p_O));
                    usedVars_f_G[i]++;
                } else if (p_S3 == 1) {
                    Genome_f_G_num[i] += ABBA - BABA;
                    Genome_f_G_denom[i] += (1-p_S1)*(1-p_O);
                    usedVars_f_G[i]++;
                }
                
                ABBAtotals[i] += ABBA;
                BABAtotals[i] += BABA;
                testTrioResults[i][0].push_back(ABBA); testTrioResults[i][1].push_back(BABA); testTrioResults[i][2].push_back(F_d_denom);
                testTrioResults[i][3].push_back(F_dM_denom); testTrioResults[i][4].push_back(stringToDouble(coord));
                testTrioResults[i][0].pop_front(); testTrioResults[i][1].pop_front(); testTrioResults[i][2].pop_front();
                testTrioResults[i][3].pop_front(); testTrioResults[i][4].pop_front();
            
            
                if (usedVars[i] > opt::windowSize && (usedVars[i] % opt::windowStep == 0)) {
                    double wABBA = vector_sum(testTrioResults[i][0]); double wBABA = vector_sum(testTrioResults[i][1]);
                    double wDnum = wABBA - wBABA; double wDdenom = wABBA + wBABA;
                    double wF_d_denom = vector_sum(testTrioResults[i][2]); double wF_dM_denom = vector_sum(testTrioResults[i][3]);
                    *outFiles[i] << chr << "\t" << testTrioResults[i][4][0] << "\t" << coord << "\t" << wDnum/wDdenom << "\t" << wDnum/wF_d_denom << "\t" << wDnum/wF_dM_denom << std::endl;
                }
            }
            durationCalculation = ( clock() - startCalculation ) / (double) CLOCKS_PER_SEC;
            delete c;

        }
    }
    
    for (int i = 0; i != testTrios.size(); i++) {
        std::cout << testTrios[i][0] << "\t" << testTrios[i][1] << "\t" << testTrios[i][2] << std::endl;
        std::cout << "D=" << (double)(ABBAtotals[i]-BABAtotals[i])/(ABBAtotals[i]+BABAtotals[i]) << std::endl;
        std::cout << "f_G=" << (double)Genome_f_G_num[i]/Genome_f_G_denom[i] << "\t" << Genome_f_G_num[i] << "/" << Genome_f_G_denom[i] << std::endl;
        std::cout << "f_d=" << (double)(ABBAtotals[i]-BABAtotals[i])/Genome_f_D_denom[i] << "\t" << (ABBAtotals[i]-BABAtotals[i]) << "/" << Genome_f_D_denom[i] << std::endl;
        std::cout << "f_dM=" << (double)(ABBAtotals[i]-BABAtotals[i])/Genome_f_DM_denom[i] << "\t" << (ABBAtotals[i]-BABAtotals[i]) << "/" << Genome_f_DM_denom[i] << std::endl;
        std::cout << std::endl;
    }
}


int abbaBabaMain(int argc, char** argv) {
    parseAbbaBabaOptions(argc, argv);
    doAbbaBaba();
    return 0;
    
}

void parseAbbaBabaOptions(int argc, char** argv) {
    bool die = false;
    std::vector<string> windowSizeStep;
    for (char c; (c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1;)
    {
        std::istringstream arg(optarg != NULL ? optarg : "");
        switch (c)
        {
            case '?': die = true; break;
            case 'w':
                windowSizeStep = split(arg.str(), ',');
                if(windowSizeStep.size() != 2) {std::cerr << "The -w option requires two arguments, separated by a comma ','\n"; exit(EXIT_FAILURE);}
                opt::windowSize = atoi(windowSizeStep[0].c_str());
                opt::windowStep = atoi(windowSizeStep[1].c_str());
                break;
            case 'n': arg >> opt::runName; break;
            case 'h':
                std::cout << ABBA_USAGE_MESSAGE;
                exit(EXIT_SUCCESS);
        }
    }
    
    if (argc - optind < 3) {
        std::cerr << "missing arguments\n";
        die = true;
    }
    else if (argc - optind > 3)
    {
        std::cerr << "too many arguments\n";
        die = true;
    }
    
    if (die) {
        std::cout << "\n" << ABBA_USAGE_MESSAGE;
        exit(EXIT_FAILURE);
    }
    
    // Parse the input filenames
    opt::vcfFile = argv[optind++];
    opt::setsFile = argv[optind++];
    opt::testTriosFile = argv[optind++];
}
