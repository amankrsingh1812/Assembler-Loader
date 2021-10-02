#include <bits/stdc++.h>
using namespace std;

char intHexMap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

ofstream fout;          //Output FileStream
ifstream fin;           //Input FileStream
int PRGOADDR;           //Starting Address of Program
int CSADDR;             //Current Control Section Address
int CSLTH;              //Current Control Section Length
vector<int> listOfCLTH; //List of length of Control Sections
int ERROR_MASK;         //Stores ERROR FLAGS {1:Duplicate External Symbol, 2:Invalid External Symbol}
int EXECADDR;           //Address from which execution of Program Starts
int progLen;            //Length of Program

//OutPut Processing Data Structures
unordered_map<int, int> machineCodeToLen;
map<int, string> outputLines;
vector<int> outputLinesKeyList;

//Hash Table Class for ESTAB
class hashTable
{
public:
    unordered_map<string, int> mp;

    bool insert(string key, int val)
    {
        if (mp.find(key) != mp.end())
            return false;
        mp.insert({key, val});
        return true;
    }

    int getVal(string key)
    {
        if (mp.find(key) == mp.end())
            return -1;
        return mp[key];
    }

} ESTAB;

//Helper function to convert Integer to Hexadecimal with 'd' digits
string intToHex(int n, int d)
{
    string ans = "";
    while (n > 0)
    {
        ans.push_back(intHexMap[n % 16]);
        n /= 16;
    }

    while (ans.size() < d)
        ans.push_back('0');
    reverse(ans.begin(), ans.end());
    return ans;
}

//Helper function to convert Hexadecimal to Integer
int hexToInt(string h)
{
    int ans = 0;
    for (auto i : h)
    {
        ans = ans * 16;
        int j = 0;
        while (intHexMap[j] != i)
            j++;
        ans = ans + j;
    }
    return ans;
}

//Helper Function to seperate name from extension of a file
void removeExtension(string &s)
{
    int i = s.size();
    i--;
    while (i >= 0 && s[i] != '.')
        i--;
    if (i < 0)
        return;
    s = s.substr(0, i);
    return;
}

//Helper Funtion to read Next Non Empty line of Object File
//Return blank when file ends
string readNextLine()
{
    string line;
    while (!fin.eof() && line.size() == 0)
        getline(fin, line);

    return line;
}

//Helper Function to get Program Name from Header Record
string getNameFromHeaderRecord(string record)
{
    string name = record.substr(1, 6);
    while (name.size() && name.back() == ' ')
        name.pop_back();

    return name;
}

//Loads OPCodes From File into Memory
void initialise()
{
    fin.open("opcodeTomachincode.txt", ios::in);
    string opcode;
    string machineCode, len;
    while (fin >> opcode)
    {
        fin >> machineCode >> len;
        machineCodeToLen[hexToInt(machineCode)] = stoi(len);
    }
    fin.close();
}

int main(int argcnt, char *args[])
{
    //Check for correct command line arguments
    if (argcnt != 3)
    {
        cout << "Please Correct number of Arguments\n";
        return 0;
    }

    //Name of the object file to load
    string fileName(args[1]);
    string intermediateFileName(args[2]);
    string line; //Store Current line read
    removeExtension(fileName);
    removeExtension(intermediateFileName);
    initialise();

    //-----------------PASS 1------------------------------
    PRGOADDR = 0;
    CSADDR = PRGOADDR;
    fin.open(fileName, ios::in);

    //Read 1st line
    line = readNextLine();
    progLen = 0;

    //LOOP Untill End of File
    while (line.size())
    {
        CSLTH = hexToInt(line.substr(13, 6));
        listOfCLTH.push_back(CSLTH);
        progLen += CSLTH;
        string csName = getNameFromHeaderRecord(line);
        if (!ESTAB.insert(csName, CSADDR))
        {
            //Duplicate External Symbol
            ERROR_MASK = (ERROR_MASK | 1);
        }

        //LOOP until End Record of Control Section
        while (line[0] != 'E')
        {
            line = readNextLine();

            //Handle Define Record
            if (line[0] == 'D')
            {
                for (int i = 1; i < line.size(); i += 12)
                {
                    string symName = line.substr(i, 6);
                    int symAddr = hexToInt(line.substr(i + 6, 6));
                    while (symName.size() && symName.back() == ' ')
                        symName.pop_back();

                    if (!ESTAB.insert(symName, CSADDR + symAddr))
                    {
                        //Duplicate External Symbol
                        ERROR_MASK = (ERROR_MASK | 1);
                    }
                }
            }
        }
        CSADDR += CSLTH;
        line = readNextLine();
    }

    fin.close();

    //-----------------PASS 2------------------------------
    fin.open(fileName, ios::in);
    fout.open(fileName + "_loaderFile", ios::out); //Create OutputFile
    // initialiseOutputFile();

    CSADDR = PRGOADDR;
    EXECADDR = PRGOADDR;

    //Read First line
    line = readNextLine();

    //LOOP Untill End of File
    while (line.size())
    {
        CSLTH = hexToInt(line.substr(13, 6));
        //LOOP until End Record of Control Sectiond
        while (line[0] != 'E')
        {
            line = readNextLine();
            //Handle Text Record
            if (line[0] == 'T')
            {
                int curAdd = CSADDR + hexToInt(line.substr(1, 6));
                for (int i = 9; i < line.size();)
                {
                    int curLen = 0;
                    int firstByte = hexToInt(line.substr(i, 2));
                    int e = 0;

                    //Find Length of instruction based on OPCODE
                    if (i + 2 < line.size())
                        e = hexToInt(line.substr(i + 2, 1)) % 2;
                    if (machineCodeToLen.find(4 * (firstByte >> 2)) != machineCodeToLen.end() && machineCodeToLen[4 * (firstByte >> 2)] == 3 && i + 5 < line.size())
                        curLen = machineCodeToLen[4 * (firstByte >> 2)] + e;
                    else if (machineCodeToLen.find(firstByte) != machineCodeToLen.end())
                        curLen = machineCodeToLen[firstByte];
                    else
                        curLen = 1;
                    outputLines.insert({curAdd, line.substr(i, curLen * 2)});
                    outputLinesKeyList.push_back(curAdd);

                    curAdd += curLen;
                    i += 2 * curLen;
                }
            }

            //Handle Modification Record
            else if (line[0] == 'M')
            {
                string symName = line.substr(10, 6);
                while (symName.back() == ' ')
                {
                    symName.pop_back();
                }

                int symVal = ESTAB.getVal(symName);
                int coeff = (line[9] == '+' ? 1 : -1);

                if (symVal != -1)
                {
                    int reqAddr = CSADDR + hexToInt(line.substr(1, 6));
                    int reqKey = outputLinesKeyList[(upper_bound(outputLinesKeyList.begin(), outputLinesKeyList.end(), reqAddr) - outputLinesKeyList.begin()) - 1];
                    int chLen = hexToInt(line.substr(7, 2));
                    int offSet = (reqAddr - reqKey) * 2 + chLen % 2;

                    string val = outputLines[reqKey];
                    string newVal = intToHex(hexToInt(val.substr(offSet, chLen)) + coeff * symVal, chLen);

                    for (int i = offSet; i < offSet + chLen; i++)
                        val[i] = newVal[i - offSet];

                    outputLines[reqKey] = val;
                }
                else
                {
                    //Undefined External Symbol
                    ERROR_MASK = (ERROR_MASK | (1 << 1));
                }
            }
        }

        if (line.size() > 1)
        {
            EXECADDR = CSADDR + hexToInt(line.substr(1, 6));
        }
        CSADDR += CSLTH;
        line = readNextLine();
    }

    fin.close();

    //Reads Intermediate file to get Line Breaks in final output
    fin.open(intermediateFileName, ios::in);

    unordered_set<int> lineBreaksAdd;
    reverse(listOfCLTH.begin(), listOfCLTH.end());
    listOfCLTH.push_back(0);
    getline(fin, line);
    int offset = 0;
    bool isNewCs = false;
    while (!fin.eof())
    {
        if (line[0] == ':')
        {
            int readAddress = hexToInt(line.substr(1, 4));
            if (readAddress == 0)
            {
                if (isNewCs)
                {
                    offset += listOfCLTH.back();
                    listOfCLTH.pop_back();
                    isNewCs = false;
                }
                else
                    isNewCs = true;
            }
            else
                isNewCs = false;
            if (readAddress + offset)
                lineBreaksAdd.insert(readAddress + offset);
        }
        getline(fin, line);
    }
    fin.close();


    //Generate Output 
    for (auto lines : outputLines)
    {
        if (lineBreaksAdd.count(lines.first))
            fout << "\n";
        fout << lines.second;
    }

    fout.close();

    if (ERROR_MASK == 0)
        cout << "Execution Sucessfull!! Program EXECADDR is " << EXECADDR << "\n";
    else
    {
        cout << "Following errors were reported\n";
        if (ERROR_MASK & 1)
            cout << "Duplicate External Symbol\n";
        if (ERROR_MASK & 2)
            cout << "Undefined External Symbol\n";
    }
}