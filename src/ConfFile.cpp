#include "ConfFile.h"


ConfFile::ConfFile()
{
}

ConfFile::ConfFile(string fName)
{
	m_fileName = fName;
}

ConfFile::~ConfFile()
{
}

void ConfFile::loadConfFile(void)
{
	vector<string> parsedLine;
	ifstream inputFile(m_fileName);
	vector<string>::const_iterator it;

	if (inputFile)
	{
		// opening the file is successful. continue by reading and parsing the file
		string line;
		while (getline(inputFile, line))
		{
			if (line != "")
			{
				// parse the line
				parseConfFileLine(line, parsedLine);

				// parse command
				parseCommand(parsedLine);

				// clear vector for next use
				parsedLine.clear();
			}
		}
		m_numSubExp = m_rotations.size();
	}
	else
	{
		cerr << "Error: Configuration file could not be opened!" << endl;
	}
}

void ConfFile::parseConfFileLine(string &l, vector<string> &parsedLine)
{
	istringstream iss(l);

	for (string s; iss >> s;)
		parsedLine.push_back(s);
}

void ConfFile::parseCommand(vector<string> &parsedLine)
{
	vector<double> tmpRot;

	if (parsedLine[0].at(0) != 35)
	{
		if (parsedLine[0] == "ROT")
		{
			if (parsedLine[1] == "RANDOM" || parsedLine[1] == "RND")
			{
				// load a random configuration for the reference dice
				tmpRot.push_back(distribution1(generator));
				tmpRot.push_back(distribution1(generator));
				tmpRot.push_back(distribution1(generator));
				tmpRot.push_back(distribution2(generator));
				m_rotations.push_back(tmpRot);
			}
			else
			{
				if (parsedLine.size() != 6)
					cerr << "Error: Wrong rotation input in the configuration file!";
				else
				{
					if (parsedLine[5] == "DEG")
					{
						// Convert rotation angle to rad
						tmpRot.push_back(stod(parsedLine[1]));
						tmpRot.push_back(stod(parsedLine[2]));
						tmpRot.push_back(stod(parsedLine[3]));
						tmpRot.push_back(stod(parsedLine[4])*DEG2RAD);
						m_rotations.push_back(tmpRot);
					}
					else if (parsedLine[5] == "RAD")
					{
						// Use radians for the rotation angle
						tmpRot.push_back(stod(parsedLine[1]));
						tmpRot.push_back(stod(parsedLine[2]));
						tmpRot.push_back(stod(parsedLine[3]));
						tmpRot.push_back(stod(parsedLine[4]));
						m_rotations.push_back(tmpRot);
					}
					else
						cerr << "Error: Wrong unit for the rotation!";
				}
			}
		}
		else if (parsedLine[0] == "ID")
		{
			if (parsedLine.size() > 2)
				cerr << "Error: Participant ID should be a single word!";
			else
				m_participantID = parsedLine[1];
		}
	}
}

void ConfFile::printConfigurations()
{
	vector<vector<double> >::const_iterator it;
	cout << "Loaded configurations:" << endl;
	
	for (it = m_rotations.begin(); it != m_rotations.end(); ++it)
		cout << (*it)[0] << "," << setw(10) << (*it)[1] << "," << setw(10) << (*it)[2] << "," << setw(15) << (*it)[3] << endl;

	cout << endl;
}

void ConfFile::openConfFile()
{
	if (m_fileName == "")
		cerr << "Error: No configuration file is given!";
	else
		loadConfFile();
}

void ConfFile::openConfFile(string s)
{
	m_fileName = s;
	loadConfFile();
}