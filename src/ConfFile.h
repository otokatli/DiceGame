#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>

using namespace std;

class ConfFile
{
public:
	string m_fileName;			// name of the configuration file
	string m_participantID;		// name/id of the participant
	vector<vector<double> > m_rotations;	// list of rotations in the configuration file (format: <vector_x, vector_y, vector_z, angle>, note that the angle is in radians)
	int m_numSubExp;				// number of subexperiments (ie number of ROT in the conf file)

public:
	ConfFile();
	ConfFile(string fName);
	~ConfFile();

public:
	void openConfFile(); // open a configuration file
	void openConfFile(string s); // open a configuration file

private:
	void loadConfFile(void);
	void parseConfFileLine(string &l, vector<string> &parsedLine);
	void parseCommand(vector<string> &parsedLine);
	void printConfigurations();

private:
	const double DEG2RAD = 0.017453292519943;
	default_random_engine generator;
	uniform_real_distribution<double> distribution1{ -1.0, 1.0 };
	uniform_real_distribution<double> distribution2{ 0.0, 6.283185307179586 };
};