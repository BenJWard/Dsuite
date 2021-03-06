//
//  Dsuite_utils.cpp
//  Dsuite
//
//  Created by Milan Malinsky on 02/04/2019.
//  Copyright © 2019 Milan Malinsky. All rights reserved.
//

#include "Dsuite_utils.h"

double normalCDF(double x) // Phi(-∞, x) aka N(x)
{
    return erfc(-x/sqrt(2))/2;
}

// Works only on biallelic markers
void GeneralSetCounts::getSetVariantCounts(const std::vector<std::string>& genotypes, const std::map<size_t, string>& posToSpeciesMap) {
    
    getBasicCounts(genotypes, posToSpeciesMap);
    
    // If at least one of the outgroup individuals has non-missing data
    // Find out what is the "ancestral allele" - i.e. the one more common in the outgroup
    int AAint;
    try {
        if (setAlleleCounts.at("Outgroup") > 0) {
            if ((double)setAltCounts.at("Outgroup")/setAlleleCounts.at("Outgroup") < 0.5) { AAint = 0; }
            else { AAint = 1; }
        }
    } catch (std::out_of_range& e) { AAint = -1; }
    
    // Now fill in the allele frequencies
    for(std::map<string,int>::iterator it = setAltCounts.begin(); it != setAltCounts.end(); ++it) {
        if (setAlleleCounts.at(it->first) > 0) {
            setAAFs[it->first] = (double)setAltCounts.at(it->first)/setAlleleCounts.at(it->first);
            if (AAint == 0) { // Ancestral allele seems to be the ref, so derived is alt
                setDAFs[it->first] = (double)setAltCounts.at(it->first)/setAlleleCounts.at(it->first);
            } else if (AAint == 1) { // Ancestral allele seems to be alt, so derived is ref
                setDAFs[it->first] = 1 - ((double)setAltCounts.at(it->first)/setAlleleCounts.at(it->first));
            }
        }
    }
}

// Works only on biallelic markers
void GeneralSetCounts::getSetVariantCountsSimple(const std::vector<std::string>& genotypes, const std::map<size_t, string>& posToSpeciesMap) {
    // std::cerr << fields[0] << "\t" << fields[1] << std::endl;
    getBasicCounts(genotypes, posToSpeciesMap);
    
    // Now fill in the allele frequencies
    for(std::map<string,int>::iterator it = setAltCounts.begin(); it != setAltCounts.end(); ++it) {
        if (setAlleleCounts.at(it->first) > 0) {
            setAAFs[it->first] = (double)setAltCounts.at(it->first)/setAlleleCounts.at(it->first);
        }
    }
}

void GeneralSetCounts::getBasicCounts(const std::vector<std::string>& genotypes, const std::map<size_t, string>& posToSpeciesMap) {
    // Go through the genotypes - only biallelic markers are allowed
    for (std::vector<std::string>::size_type i = 0; i != genotypes.size(); i++) {
        std::string species = posToSpeciesMap.at(i);
        // The first allele in this individual
        if (genotypes[i][0] == '1') {
            overall++; individualsWithVariant[i]++;
            setAltCounts[species]++; setAlleleCounts[species]++;
        } else if (genotypes[i][0] == '0') {
            setAlleleCounts[species]++; 
        }
        // The second allele in this individual
        if (genotypes[i][2] == '1') {
            overall++;
            setAltCounts[species]++; setAlleleCounts[species]++;
            individualsWithVariant[i]++;
        } else if (genotypes[i][2] == '0') {
            setAlleleCounts[species]++;
        }
    }
}

void GeneralSetCountsWithSplits::getBasicCounts(const std::vector<std::string>& genotypes, const std::map<size_t, string>& posToSpeciesMap) {
    // Go through the genotypes - only biallelic markers are allowed
    for (std::vector<std::string>::size_type i = 0; i != genotypes.size(); i++) {
        double r = ((double) rand() / (RAND_MAX));
        std::string species = posToSpeciesMap.at(i);
        // The first allele in this individual
        if (genotypes[i][0] == '1') {
            overall++; individualsWithVariant[i]++;
            setAltCounts[species]++; setAlleleCounts[species]++;
            if (r < 0.5) {
                setAltCountsSplit1[species]++; setAlleleCountsSplit1[species]++;
            } else {
                setAltCountsSplit2[species]++; setAlleleCountsSplit2[species]++;
            }
        } else if (genotypes[i][0] == '0') {
            setAlleleCounts[species]++;
            if (r < 0.5) {
                setAlleleCountsSplit1[species]++;
            } else {
                setAlleleCountsSplit2[species]++;
            }
        }
        // The second allele in this individual
        if (genotypes[i][2] == '1') {
            overall++; individualsWithVariant[i]++;
            setAltCounts[species]++; setAlleleCounts[species]++;
            if (r < 0.5) {
                setAltCountsSplit1[species]++; setAlleleCountsSplit1[species]++;
            } else {
                setAltCountsSplit2[species]++; setAlleleCountsSplit2[species]++;
            }
        } else if (genotypes[i][2] == '0') {
            setAlleleCounts[species]++;
            if (r < 0.5) {
                setAlleleCountsSplit1[species]++;
            } else {
                setAlleleCountsSplit2[species]++;
            }
        }
    }
}

void GeneralSetCountsWithSplits::getSplitCounts(const std::vector<std::string>& genotypes, const std::map<size_t, string>& posToSpeciesMap) {
    
    getBasicCounts(genotypes, posToSpeciesMap);
    
    // If at least one of the outgroup individuals has non-missing data
    // Find out what is the "ancestral allele" - i.e. the one more common in the outgroup
    int AAint;
    try {
        if (setAlleleCounts.at("Outgroup") > 0) {
            if ((double)setAltCounts.at("Outgroup")/setAlleleCounts.at("Outgroup") < 0.5) { AAint = 0; }
            else { AAint = 1; }
        }
    } catch (std::out_of_range& e) { AAint = -1; }
    
    // Now fill in the allele frequencies
    for(std::map<string,int>::iterator it = setAltCounts.begin(); it != setAltCounts.end(); ++it) {
        if (setAlleleCounts.at(it->first) > 0) {
            setAAFs[it->first] = (double)setAltCounts.at(it->first)/setAlleleCounts.at(it->first);
            int nSplit1 = setAlleleCountsSplit1.at(it->first); int nSplit2 = setAlleleCountsSplit2.at(it->first);
            if (nSplit1 > 0)
                setAAFsplit1[it->first] = (double)setAltCountsSplit1.at(it->first)/nSplit1;
            if (nSplit2 > 0)
                setAAFsplit2[it->first] = (double)setAltCountsSplit2.at(it->first)/nSplit2;
            if (AAint == 0) { // Ancestral allele seems to be the ref, so derived is alt
                setDAFs[it->first] = (double)setAltCounts.at(it->first)/setAlleleCounts.at(it->first);
                if (nSplit1 > 0)
                    setDAFsplit1[it->first] = (double)setAltCountsSplit1.at(it->first)/nSplit1;
                if (nSplit2 > 0)
                    setDAFsplit2[it->first] = (double)setAltCountsSplit2.at(it->first)/nSplit2;
            } else if (AAint == 1) { // Ancestral allele seems to be alt, so derived is ref
                setDAFs[it->first] = 1 - ((double)setAltCounts.at(it->first)/setAlleleCounts.at(it->first));
                if (nSplit1 > 0)
                    setDAFsplit1[it->first] = 1 - ((double)setAltCountsSplit1.at(it->first)/nSplit1);
                if (nSplit2 > 0)
                    setDAFsplit2[it->first] = 1 - ((double)setAltCountsSplit2.at(it->first)/nSplit2);
            }
        }
    }
}



double stringToDouble(std::string s) {
    double d;
    std::stringstream ss(s); //turn the string into a stream
    ss >> d; //convert
    return d;
}


// Remove a single file extension from the filename
std::string stripExtension(const std::string& filename)
{
    size_t suffixPos = filename.find_last_of('.');
    if(suffixPos == std::string::npos)
        return filename; // no suffix
    else
        return filename.substr(0, suffixPos);
}


void split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

std::vector<size_t> locateSet(std::vector<std::string>& sample_names, const std::vector<std::string>& set) {
    std::vector<size_t> setLocs;
    for (std::vector<std::string>::size_type i = 0; i != set.size(); i++) {
        std::vector<std::string>::iterator it = std::find(sample_names.begin(), sample_names.end(), set[i]);
        if (it == sample_names.end()) {
            std::cerr << "Did not find the sample: \"" << set[i] << "\"" << std::endl;
            std::cerr << "Did not find the sample: \"" << sample_names[43] << "\"" << std::endl;
            print_vector(sample_names, std::cerr,',');
        } else {
            size_t loc = std::distance(sample_names.begin(), it);
            setLocs.push_back(loc);
        }
    }
    return setLocs;
}

//
std::string suffix(const std::string& seq, size_t len)
{
    assert(seq.length() >= len);
    return seq.substr(seq.length() - len);
}

// Returns true if the filename has an extension indicating it is compressed
bool isGzip(const std::string& filename)
{
    size_t suffix_length = sizeof(GZIP_EXT) - 1;
    
    // Assume files without an extension are not compressed
    if(filename.length() < suffix_length)
        return false;
    
    std::string extension = suffix(filename, suffix_length);
    return extension == GZIP_EXT;
}

// Ensure a filehandle is open
void assertFileOpen(std::ifstream& fh, const std::string& fn)
{
    if(!fh.is_open())
    {
        std::cerr << "Error: could not open " << fn << " for read\n";
        exit(EXIT_FAILURE);
    }
}
// Ensure a filehandle is open
void assertFileOpen(std::ofstream& fh, const std::string& fn)
{
    if(!fh.is_open())
    {
        std::cerr << "Error: could not open " << fn << " for write\n";
        exit(EXIT_FAILURE);
    }
}


void assertGZOpen(gzstreambase& gh, const std::string& fn)
{
    if(!gh.good())
    {
        std::cerr << "Error: could not open " << fn << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Open a file that may or may not be gzipped for reading
// The caller is responsible for freeing the handle
std::istream* createReader(const std::string& filename, std::ios_base::openmode mode)
{
    if(isGzip(filename))
    {
        igzstream* pGZ = new igzstream(filename.c_str(), mode);
        assertGZOpen(*pGZ, filename);
        return pGZ;
    }
    else
    {
        std::ifstream* pReader = new std::ifstream(filename.c_str(), mode);
        assertFileOpen(*pReader, filename);
        return pReader;
    }
}

// Open a file that may or may not be gzipped for writing
// The caller is responsible for freeing the handle
std::ostream* createWriter(const std::string& filename,
                           std::ios_base::openmode mode)
{
    if(isGzip(filename))
    {
        ogzstream* pGZ = new ogzstream(filename.c_str(), mode);
        assertGZOpen(*pGZ, filename);
        return pGZ;
    }
    else
    {
        std::ofstream* pWriter = new std::ofstream(filename.c_str(), mode);
        assertFileOpen(*pWriter, filename);
        return pWriter;
    }
}

bool file_exists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}
