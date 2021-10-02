#include <bits/stdc++.h>
using namespace std;

char intHexMap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

ofstream fout;               //Output FileStream
ifstream fin;                //Input FileStream
int LOCCTR;                  //Stores address of currently read instruction
int startingAddress;         //Stores Starting address of program
string programName;          //Stores program name as given in assembly file
string curentTextRecord;     //Stores current incomplete text record
int ERROR_MASK;              //Stores ERROR FLAGS {1:Duplicate LABEL, 2:Invalid OPCODE, 4:Invalid LABEL}
int csCnt;                   //Stores Count of Control Sections
int BASE;                    //Base Register Value
vector<string> csName;       //Current Control Section Value
vector<int> csLength;        //Length of Current Control Section
vector<string> literalsList; //List of Literals
map<char, char> REGTAB;      //Stores Machine code for registers

//Class to represent single assemble instruction
class Instruction
{
public:
    string LABEL;
    string OPCODE;
    string OPERAND;
    int n, i, x, e;
    bool isLiteralOperand;
    //Helper function to remove starting and ending whiteSpace
    void removeWhiteSpace(string &s)
    {
        while (s.size() && s.back() == ' ')
            s.pop_back();
        while (s.size() && s[0] == ' ')
            s.erase(s.begin());
    }

    Instruction(string line)
    {
        LABEL = OPCODE = OPERAND = "";
        x = e = 0;
        n = i = 1;
        isLiteralOperand = false;

        LABEL = line.substr(0, 10);
        if (line.size() > 10)
            OPCODE = line.substr(10, 10);
        if (line.size() > 20)
            OPERAND = line.substr(20, 30);
        removeWhiteSpace(LABEL);
        removeWhiteSpace(OPCODE);
        removeWhiteSpace(OPERAND);
        if (LABEL[0] == '.')
        {
            OPCODE = ".";
            OPERAND = LABEL = "";
        }

        if (OPCODE[0] == '+')
        {
            e = 1;
            OPCODE.erase(OPCODE.begin());
        }
        if (OPERAND.size() > 1 && OPERAND[(int)OPERAND.size() - 2] == ',' && OPERAND.back() == 'X')
        {
            OPERAND.pop_back();
            OPERAND.pop_back();
            x = 1;
        }
        if (OPERAND[0] == '#')
        {
            i = 1;
            n = 0;
            OPERAND.erase(OPERAND.begin());
        }
        else if (OPERAND[0] == '@')
        {
            i = 0;
            n = 1;
            OPERAND.erase(OPERAND.begin());
        }
        else if (OPERAND[0] == '=')
        {
            isLiteralOperand = true;
            OPERAND.erase(OPERAND.begin());
        }

        transform(OPCODE.begin(), OPCODE.end(), OPCODE.begin(), ::toupper);
    }
};

//HashTable class for implementing EXTREFTAB and OPTAB
class hashTable
{
public:
    unordered_map<string, pair<int, string>> mp;

    bool insert(string key, pair<int, string> val)
    {
        if (mp.find(key) != mp.end())
            return false;
        mp.insert({key, val});
        return true;
    }

    pair<int, string> getVal(string key)
    {
        if (mp.find(key) == mp.end())
            return {-1, ""};
        return mp[key];
    }

} EXTREFTAB, OPTAB;

//Class to Store information of Literals and Symbols
class info
{
public:
    char type;
    int Address;
    info()
    {
        type = 0;
        Address = -1;
    }
    info(char addType, int address)
    {
        type = addType;
        Address = address;
    }
};

//HashTable class for implementing SYMTAB and LITTAB
class symbolHashTable
{

public:
    map<string, info> mp;
    void clear()
    {
        mp.clear();
    }

    bool isPresent(string symbol, int csId)
    {
        symbol += to_string(csId);
        return mp.find(symbol) != mp.end();
    }
    bool insert(string symbol, char addType, int csId, int address)
    {
        symbol += to_string(csId);
        if (mp.find(symbol) != mp.end() && mp[symbol].type != ' ')
        {
            return false;
        }
        mp[symbol] = info(addType, address);
        return true;
    }

    int getValue(int csId, string symbol)
    {
        symbol += to_string(csId);
        if (mp.find(symbol) == mp.end())
            return INT_MIN;
        return mp[symbol].Address;
    }
    int getTargetAddress(int csId, int type, int LOCCTR, int BASE, string symbol)
    {
        symbol += to_string(csId);
        if (mp.find(symbol) == mp.end())
            return INT_MIN;
        if (type == 4 || mp[symbol].type == 'A')
        {
            return mp[symbol].Address;
        }
        int ta = mp[symbol].Address - LOCCTR;
        if (ta >= -2048 && ta <= 2047)
            return ta;
        return mp[symbol].Address - BASE;
    }
} SYMTAB, LITTAB;

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

//Checks whether a Char is Hexadecimal or not
bool ishex(char c)
{
    if (isdigit(c))
        return true;
    if (c - 'A' >= 0 && c - 'A' <= 5)
        return true;
    return false;
}

//Initialises an empty new text record
string initialiseTextRecord()
{
    string textRecord = "T";
    textRecord += intToHex(LOCCTR, 6);
    textRecord += "00";
    return textRecord;
}

//Reads next line from input and converts it as instruction class objects
Instruction readNextLine(string &line)
{
    getline(fin, line);
    int i = 0, n = line.size();
    if (line[i] == ':')
    {
        i++;
        while (i < line.size() && ishex(line[i]))
            i++;
    }
    if (i > 0)
        LOCCTR = hexToInt(line.substr(1, i - 1));

    line = line.substr(i, n - i);
    return Instruction(line);
}

// Helper funtion to Load OPTABLE into memory
void initialise()
{
    LITTAB.clear();
    SYMTAB.clear();
    REGTAB['A'] = '0';
    REGTAB['X'] = '1';
    REGTAB['B'] = '3';
    REGTAB['S'] = '4';
    REGTAB['T'] = '5';
    REGTAB['F'] = '6';
    fin.open("opcodeTomachincode.txt", ios::in);
    string opcode, machinecode, type;
    while (fin >> opcode)
    {
        fin >> machinecode >> type;
        OPTAB.insert(opcode, {hexToInt(machinecode), type});
    }
    fin.close();
}

//Checks whether more code can be added to current text record
bool isPossibleToAddObjCode(string objCode)
{
    int expectedLOCCTR = hexToInt(curentTextRecord.substr(7, 2)) + hexToInt(curentTextRecord.substr(1, 6));
    if (expectedLOCCTR != LOCCTR)
        return false;

    if ((int)objCode.size() + (int)curentTextRecord.size() > 69)
        return false;

    return true;
}

//Helper function to add object code to current text record
void addObjCodeToCurentTextRecord(string objCode)
{
    curentTextRecord += objCode;
    int textRecordSize = hexToInt(curentTextRecord.substr(7, 2));
    textRecordSize += ((int)objCode.size()) / 2;
    string textRecordSizeNewHex = intToHex(textRecordSize, 2);
    curentTextRecord[7] = textRecordSizeNewHex[0];
    curentTextRecord[8] = textRecordSizeNewHex[1];
}

//Returns size in Bytes of literal
int getLiteralSize(string inpLiteral)
{
    int ans = inpLiteral.size();
    ans -= 3;
    if (inpLiteral[0] == '=')
        ans--;
    if (inpLiteral[1] == 'X' || inpLiteral[0] == 'X')
        ans = ans / 2;
    return ans;
}

//Writes to file Unwritten symbols Literalpool
//Called upon LTORG instruction OR end of a control section
void outputLiteralPOOL(int csId)
{
    while (literalsList.size())
    {
        string curLiteral = literalsList.back();
        literalsList.pop_back();
        fout << ":" << intToHex(LOCCTR, 4) << " ";
        int cnt = 10;
        while (cnt--)
            fout << " ";
        fout << "LITERAL";
        cnt = 3;
        while (cnt--)
            fout << " ";
        fout << curLiteral << "\n";
        int literalSize = getLiteralSize(curLiteral);
        LITTAB.insert(curLiteral, 'R', csId, LOCCTR);
        LOCCTR += literalSize;
    }
}

//Checks whether operand is a Constant
bool isConstOperand(string s)
{
    for (auto c : s)
    {
        if (!(c - '0' >= 0 && c - '0' <= 9))
            return false;
    }
    return true;
}

//Returns pair Operand value and int
//Second element is 1 for external symbols
//0 otherwise
pair<int, int> findOperandValue(string operand, int csId)
{
    if (isConstOperand(operand))
    {
        return {stoi(operand), 0};
    }

    int symTabValue = SYMTAB.getValue(csId, operand);
    if (symTabValue != INT_MIN)
        return {symTabValue, 0};

    if (EXTREFTAB.getVal(operand).first != -1)
        return {0, 1};

    return {INT_MIN, 0};
}

//Writes Refer/Define Record(choice indicated by boolean isDef) in the output file
void outputReferDefineRecord(string operand, bool isDef, int csId)
{
    string sym = "";
    operand.push_back(',');
    int symCnt = 0;
    if (isDef)
        fout << "D";
    else
        fout << "R";
    for (auto c : operand)
    {
        if (c == ',')
        {
            symCnt++;
            if (symCnt == 7)
            {
                fout << "\nD";
                symCnt = 1;
            }
            for (int i = 0; i < 6; i++)
                if (i < sym.size())
                    fout << sym[i];
                else
                    fout << " ";
            if (isDef)
                fout << intToHex(SYMTAB.getTargetAddress(csId, 'R', LOCCTR, BASE, sym), 6);
            sym = "";
        }
        else
            sym += c;
    }
    fout << "\n";
}

//Outputs current text record,all modification records,end record
void outputEndPartCS(vector<string> &modificationRecords, string outputSA)
{
    fout << curentTextRecord << "\n";
    for (auto &record : modificationRecords)
    {
        while (record.size() < 15)
        {
            record += ' ';
        }

        fout << "M" << record << "\n";
    }
    modificationRecords.clear();
    fout << "E";
    fout << outputSA;
    fout << "\n\n";
}

//Returns value of an expression along with list of symbols with signs of the expression
pair<int, vector<string>> evaluateExpression(string operand)
{
    int reqAdd = 0;
    vector<string> mList;
    string sym = "";
    int coeff = 1;
    operand.push_back('+');
    for (auto c : operand)
    {
        if (c == '-' || c == '+')
        {
            auto p = findOperandValue(sym, csCnt - 1);
            if (p.second)
            {
                string sgn = "+";
                if (coeff == -1)
                    sgn = "-";
                mList.push_back(sgn + sym);
            }
            int g = p.first;
            if (g != INT_MIN)
            {
                reqAdd += coeff * g;
            }
            else
            {
                //ERROR
            }
            coeff = 1;
            if (c == '-')
                coeff = -1;
            sym.clear();
        }
        else
            sym += c;
    }
    return {reqAdd, mList};
}

//Converts bitset to corresponding hex value
string bitsetToHex(bitset<32> b, int n)
{
    string ans = "";
    int num = 0;
    for (int i = 0; i < n; i++)
    {
        if (i > 0 && i % 4 == 0)
        {
            ans += intHexMap[num];
            num = 0;
        }
        num = num * 2;
        num += b[i];
    }
    ans += intHexMap[num];
    return ans;
}

int main(int argcnt, char *args[])
{
    //Check for correct command line arguments
    if (argcnt == 1)
    {
        cout << "Please Input File Name";
        return 0;
    }

    //Name of the file to assemble
    string fileName(args[1]);

    //Load OPTABLE into memory
    initialise();

    //-----------------PASS 1------------------------------
    fin.open(fileName, ios::in);
    removeExtension(fileName);
    cout << "Assembling File:" << fileName << "\n";
    fout.open(fileName + "_Intermediate", ios::out); // Create Intermediate file

    string line; // Stores currently read line

    Instruction instruction = readNextLine(line);

    //Loop utill END of file
    while (instruction.OPCODE.compare("END"))
    {
        bool toPrint = true;

        //Handle Start/Csect instruction
        if (instruction.OPCODE == "START" || instruction.OPCODE == "CSECT")
        {
            if (instruction.OPCODE == "CSECT")
            {
                outputLiteralPOOL(csCnt - 1);
                csLength.push_back(LOCCTR - startingAddress);
            }
            startingAddress = hexToInt(instruction.OPERAND);
            LOCCTR = startingAddress;
            csCnt++;

            csName.push_back(instruction.LABEL);
            EXTREFTAB.insert(csName.back(), {csCnt - 1, csName.back()});
            fout << ":" << intToHex(LOCCTR, 4) << " ";
        }

        //Handle EXTDEF instruction
        else if (instruction.OPCODE == "EXTDEF")
        {
            string word;
            for (auto c : instruction.OPERAND)
            {
                if (c == ',')
                {
                    EXTREFTAB.insert(word, {csCnt - 1, csName.back()});
                    word = "";
                }
                else
                    word += c;
            }
            if (word.size())
                EXTREFTAB.insert(word, {csCnt - 1, csName.back()});
        }

        else if (instruction.OPCODE.size() > 0 && instruction.OPCODE.compare(".") && instruction.OPCODE.compare("EXTREF"))
        {

            //Handles LTORG instruction
            if (instruction.OPCODE == "LTORG")
            {
                toPrint = false;
                outputLiteralPOOL(csCnt - 1);
            }

            //Handles EQU assembler directive
            else if (instruction.OPCODE == "EQU")
            {
                int reqAdd = 0;
                if (instruction.OPERAND == "*")
                {
                    reqAdd = LOCCTR;
                }
                else
                {
                    reqAdd = evaluateExpression(instruction.OPERAND).first;
                }
                if (SYMTAB.insert(instruction.LABEL, 'A', csCnt - 1, reqAdd) == false)
                {
                    //error
                }
            }

            //Handles sic-xe instructions/word/resb/byte/resw
            else
            {
                fout << ":" << intToHex(LOCCTR, 4) << " ";
                if (instruction.LABEL.size() > 0)
                {
                    if (SYMTAB.insert(instruction.LABEL, 'R', csCnt - 1, LOCCTR) == false)
                    {
                        ERROR_MASK = (ERROR_MASK | 1);
                    }
                }
                if (OPTAB.getVal(instruction.OPCODE).first != -1)
                    LOCCTR += stoi(OPTAB.getVal(instruction.OPCODE).second) + instruction.e;
                else if (instruction.OPCODE == "WORD")
                    LOCCTR += 3;
                else if (instruction.OPCODE == "RESW")
                    LOCCTR += 3 * stoi(instruction.OPERAND);
                else if (instruction.OPCODE == "RESB")
                    LOCCTR += stoi(instruction.OPERAND);
                else if (instruction.OPCODE == "BYTE")
                    LOCCTR += getLiteralSize(instruction.OPERAND);
                else
                {
                    ERROR_MASK = (ERROR_MASK | (1 << 1));
                }

                if (instruction.isLiteralOperand && !LITTAB.isPresent(instruction.OPERAND, csCnt - 1))
                {
                    literalsList.push_back(instruction.OPERAND);
                    LITTAB.insert(instruction.OPERAND, ' ', csCnt - 1, INT_MIN);
                }
            }
        }
        if (toPrint)
            fout << line << "\n";
        instruction = readNextLine(line);
    }
    outputLiteralPOOL(csCnt - 1);
    csLength.push_back(LOCCTR - startingAddress);

    fout << line << "\n";

    fin.close();
    fout.close();
    // PASS 1 Ends

    // //-----------------PASS 2------------------------------
    fin.open(fileName + "_Intermediate", ios::in);
    fout.open(fileName + "_objectProgram", ios::out); //Create OutputFile

    int csId = -1;
    string outputSA = "";
    vector<string> modificationRecords;
    instruction = readNextLine(line);

    //LOOP till END of File
    while (instruction.OPCODE.compare("END"))
    {
        if (instruction.OPCODE == "START" || instruction.OPCODE == "CSECT")
        {
            //Complete OLD
            if (instruction.OPCODE == "CSECT")
                outputEndPartCS(modificationRecords, outputSA);

            //Begin New
            curentTextRecord = initialiseTextRecord();
            csId++;
            startingAddress = hexToInt(instruction.OPERAND);
            outputSA = "";
            if (instruction.OPERAND.size() > 0)
                outputSA = intToHex(startingAddress, 6);

            //Output Header Record
            fout << "H" << csName[csId];
            int j = 6 - (int)csName[csId].size();
            while (j--)
                fout << " ";
            fout << intToHex(startingAddress, 6) << intToHex(csLength[csId], 6) << "\n";
        }
        else if (instruction.OPCODE == "EXTDEF")
            outputReferDefineRecord(instruction.OPERAND, true, csId);
        else if (instruction.OPCODE == "EXTREF")
            outputReferDefineRecord(instruction.OPERAND, false, csId);
        else if (instruction.OPCODE == "RSUB")
        {
            string objCode = "4F0";
            objCode += intToHex(startingAddress, 3);
            if (!isPossibleToAddObjCode(objCode))
            {
                fout << curentTextRecord << "\n";
                curentTextRecord = initialiseTextRecord();
            }
            addObjCodeToCurentTextRecord(objCode);
        }
        else if (instruction.OPCODE.size() > 0 && instruction.OPCODE.compare(".") && instruction.OPCODE.compare("EQU") && instruction.OPCODE.compare("RESB") && instruction.OPCODE.compare("RESW"))
        {
            //OPCODE,LITERAL,WORD,BYTE
            string objCode = "";
            auto opCode = OPTAB.getVal(instruction.OPCODE);
            if (opCode.first != -1)
            {
                if (opCode.second == "2")
                {
                    objCode = intToHex(opCode.first, 2);
                    for (auto reg : instruction.OPERAND)
                    {
                        if (reg != ',')
                            objCode += REGTAB[reg];
                    }
                    while (objCode.size() < 4)
                        objCode += "0";
                }
                else
                {
                    int opCodeDec = opCode.first >> 2;
                    bitset<32> objCodeBinary;
                    for (int i = 5; i >= 0; i--)
                    {
                        objCodeBinary[i] = opCodeDec % 2;
                        opCodeDec /= 2;
                    }

                    objCodeBinary[6] = instruction.n;
                    objCodeBinary[7] = instruction.i;
                    objCodeBinary[8] = instruction.x;
                    objCodeBinary[11] = instruction.e;

                    int reqAdd = 0;
                    bool isMRecord = false;
                    bool isPCadd = true;

                    if (isConstOperand(instruction.OPERAND))
                        reqAdd = stoi(instruction.OPERAND);
                    else if (instruction.isLiteralOperand)
                    {
                        reqAdd = LITTAB.getTargetAddress(csId, 3 + instruction.e, LOCCTR + 3 + instruction.e, BASE, instruction.OPERAND);
                        if (reqAdd > 2047)
                        {
                            objCodeBinary[9] = 1;
                            objCodeBinary[10] = 0;
                            isPCadd = false;
                        }
                        else
                        {
                            objCodeBinary[9] = 0;
                            objCodeBinary[10] = 1;
                        }
                    }
                    else if (SYMTAB.isPresent(instruction.OPERAND, csId))
                    {
                        reqAdd = SYMTAB.getTargetAddress(csId, 3 + instruction.e, LOCCTR + 3 + instruction.e, BASE, instruction.OPERAND);
                        if (reqAdd > 2047)
                        {
                            objCodeBinary[9] = 1;
                            objCodeBinary[10] = 0;
                            isPCadd = false;
                        }
                        else
                        {
                            objCodeBinary[9] = 0;
                            objCodeBinary[10] = 1;
                        }
                    }
                    else if (EXTREFTAB.getVal(instruction.OPERAND).first != -1)
                    {
                        string modificationRecord = intToHex(LOCCTR + 1, 6) + "05+" + instruction.OPERAND;
                        modificationRecords.push_back(modificationRecord);
                        reqAdd = 0;
                        isMRecord = true;
                    }
                    else
                    {
                        ERROR_MASK = (ERROR_MASK | (1 << 2));
                    }

                    if (instruction.e)
                    {
                        objCodeBinary[9] = objCodeBinary[10] = 0;
                        if (!isMRecord)
                        {
                            string modificationRecord = intToHex(LOCCTR + 1, 6) + "05+" + csName[csId];
                            modificationRecords.push_back(modificationRecord);
                        }
                        for (int i = 31; i > 11; i--)
                        {
                            objCodeBinary[i] = reqAdd % 2;
                            reqAdd /= 2;
                        }
                    }
                    else
                    {
                        if (isPCadd)
                        {
                            bool tdFlip = false;
                            if (reqAdd < 0)
                            {
                                objCodeBinary[12] = 1;
                                reqAdd = -reqAdd;
                                tdFlip = true;
                            }
                            for (int i = 23; i > 12; i--)
                            {
                                if (!tdFlip)
                                    objCodeBinary[i] = reqAdd % 2;
                                else
                                    objCodeBinary[i] = 1 - reqAdd % 2;

                                reqAdd /= 2;
                            }
                            if (tdFlip)
                            {
                                int y = 0;
                                for (int i = 23; i > 12; i--)
                                {
                                    if (objCodeBinary[i] == 0)
                                    {
                                        objCodeBinary[i] = 1;
                                        break;
                                    }
                                    objCodeBinary[i] = 0;
                                }
                            }
                        }
                        else
                        {
                            for (int i = 23; i > 11; i--)
                            {
                                objCodeBinary[i] = reqAdd % 2;
                                reqAdd /= 2;
                            }
                        }
                    }

                    objCode = bitsetToHex(objCodeBinary, 24 + 8 * instruction.e);
                }
            }
            else if (instruction.OPCODE == "LITERAL" || instruction.OPCODE == "BYTE")
            {
                if (instruction.OPERAND[0] == 'X')
                    objCode += instruction.OPERAND.substr(2, (int)instruction.OPERAND.size() - 3);
                else if (instruction.OPERAND[0] == 'C')
                {
                    int i = 2;
                    while (i + 1 < instruction.OPERAND.size())
                    {
                        objCode += intToHex((int)instruction.OPERAND[i], 2);
                        i++;
                    }
                }
            }
            else if (instruction.OPCODE == "WORD")
            {
                auto p = evaluateExpression(instruction.OPERAND);
                objCode = intToHex(p.first, 6);
                for (auto extSym : p.second)
                {
                    string modificationRecord = intToHex(LOCCTR, 6) + "06" + extSym;
                    modificationRecords.push_back(modificationRecord);
                }
            }
            else
            {
                ERROR_MASK = (ERROR_MASK | (1 << 1));
            }

            if (!isPossibleToAddObjCode(objCode))
            {
                fout << curentTextRecord << "\n";
                curentTextRecord = initialiseTextRecord();
            }
            addObjCodeToCurentTextRecord(objCode);
        }
        instruction = readNextLine(line);
    }
    outputEndPartCS(modificationRecords, outputSA);

    fin.close();
    fout.close();

    if (ERROR_MASK > 0)
    {
        cout << "Following Errors were Reported\n";
        if ((ERROR_MASK & 1))
            cout << "Duplicate LABEL\n";
        if ((ERROR_MASK & 2))
            cout << "Invalid OPCODE\n";
        if ((ERROR_MASK & 4))
            cout << "Invalid LABEL\n";
    }
    else
    {
        cout << "Assembler executed Sucessfully!!!\n";
        cout << "Output File: " << fileName + "_objectProgram\n";
    }
    return 0;
}