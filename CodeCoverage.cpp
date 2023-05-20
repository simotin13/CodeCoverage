#include <cmath>
#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <regex>

#include "pin.H"
#include "util.h"

struct EFlags
{    
    bool OF;    // Overflow Flag(1: overflow, 0: no overflow)
    bool DF;    // Direction Flag(1: down, 0: up)
    bool IF;    // Interrupt Enable Flag(1: enabled, 0: disabled )
    bool TF;    // Trap Flag (1: single step, 0: normal)
    bool SF;    // Sign Flag (1: negative, 0: positive)
    bool ZF;    // Zero Flag (1: zero, 0: non-zero)
    bool AF;    // Auxiliary Carry Flag (1: carry, 0: no carry)
    bool PF;    // Parity Flag (1: even, 0: odd)
    bool CF;    // Carry Flag (1: carry, 0: no carry)
};

struct LineInfo
{
    UINT32 LineNumber;
    std::string Text;
    bool Executable;
    bool Covered;
};

struct FuncInfo
{
    std::string Name;
    ADDRINT Addr;
    UINT32 Size;
};

enum class InsType
{
    BranchOrCall,
    Ret,
    Other
};

enum class OperandType
{
    Reg,
    Memory,
    Immediate
};

class Operand
{
public:
    OperandType Type;
    bool IsRead;
    bool IsWrite;
    bool IsSrc;
    bool IsDst;
};

class MemoryOperand : public Operand
{
public:
    REG BaseReg;
    INT32 offset;
};

class ImmediateOperand : public Operand
{
public:
    INT64 ImmediateValue;
};

class RegOperand : public Operand
{
public:
    REG Reg;
};

class InsInfo
{
public:
    InsType Type;
    ADDRINT Addr;
    USIZE Size;
    std::string Disassemble;
    ADDRINT NextAddr;
    OPCODE Opcode;
    std::vector<Operand *> Operands;

public:
    virtual ADDRINT Execute(EFlags &eflags)
    {
        return Addr + Size;
    }
};

class InsCmp : public InsInfo
{
public:
};

class InsJnle : public InsInfo
{
public:
    ADDRINT GetBranchAddr(EFlags &eflags)
    {
        ADDRINT branchAddr = NextAddr;
        if (eflags.ZF == 0 && eflags.SF)
        {
            //branchAddr = Operands[0]->ImmediateValue;
        }
        return branchAddr;
    }
};

struct BasicBlockInfo
{
    ADDRINT Start;
    bool Executed;
    std::vector<InsInfo *> InsList;
    std::vector<ADDRINT> nextBlockAddrList;
};

struct FuncCodeCoverage
{
    std::string Name;
    std::map<ADDRINT, INT32> AddrLineMap;
    std::map<ADDRINT, std::string> AddrAsmMap;
    std::map<INT32, bool> LineCoveredMap;
    std::map<ADDRINT, bool> InsCoveredMap;
    std::vector<BasicBlockInfo> BasicBlocks;
    UINT32 TotalLineCount;
    UINT32 CoveredLineCount;
};

struct FileCodeCoverage
{
    std::string FilePath;
    std::map<std::string, FuncCodeCoverage> FuncCodeCoverageMap;
    std::vector<LineInfo> Lines;
};

// =====================================================================
// Global Variables
// =====================================================================
static std::string s_targetName;
static std::map<std::string, FileCodeCoverage> s_fileCodeCoverageMap;

static std::map<std::string, std::string> s_funcFileMap;
static std::map<ADDRINT, std::string> s_addrFuncNameMap;

static bool isBlockEnd(INS ins)
{
    INT32 category = INS_Category(ins);
    if (category == XED_CATEGORY_COND_BR)
    {
        return true;
    }
    if (category == XED_CATEGORY_CALL)
    {
        return true;
    }
    if (category == XED_CATEGORY_RET )
    {
        return true;
    }
    return false;
}

static InsInfo *makeInsInfo(INS ins)
{
    InsInfo *pInsInfo;
    OPCODE opcode = INS_Opcode(ins);
    switch (opcode)
    {
    case XED_ICLASS_NOP:
    {
        pInsInfo = new InsInfo();
        pInsInfo->Type = InsType::Other;
    }
    break;
    {
        assert(false);
    }
    break;
    case XED_ICLASS_PUSH:
    {
    }
    break;

    case XED_ICLASS_CALL_NEAR:
    {
        assert(false);
    }
    break;
    case XED_ICLASS_RET_NEAR:
    {
        assert(false);
    }
    break;

    case XED_ICLASS_LEA:
    {
        assert(false);
    }
    break;

    case XED_ICLASS_ADD:
    {
        assert(false);
    }
    break;
    case XED_ICLASS_SUB:
    {
        // P409
        // Subtracts the second operand (source operand) from the first operand and stores the result in the destination operand
        // The destination operand can be a register or a memory location; the source operand can be an immediate, register, or memory location.
        // (However, two memory operands cannot be used in one instruction.)
        // When an immediate value is used as an operand, it is sign-extended to the length of the destination operand format.
        // Flags Affected
        // The OF, SF, ZF, AF, PF, and CF flags are set according to the result.
    }
    break;
    case XED_ICLASS_MOV:
    {
    }
    break;

    case XED_ICLASS_CMP:
    {
        // P289
        pInsInfo = new InsInfo();
        pInsInfo->Type = InsType::Other;

        // check dst operand, src operand
        // operand dst: memory or register
        // operand src: register or immediate
        // savke address and register
        UINT32 cnt = INS_OperandCount(ins);
        printf("INS_OperandCount: %d\n", cnt);
        for(UINT32 i = 0; i < cnt; i++)
        {
            if (INS_OperandRead(ins, i))
            {
                // read operand
                printf("%d:read operand\n", i);
            }
            if (INS_OperandWritten(ins, i))
            {
                // write operand
                printf("%d:write operand\n", i);
            }
            if(INS_OperandIsMemory(ins, i))
            {
                // memory operand
                // save memory address
                printf("memory operand\n");
                ADDRDELTA displacement = INS_OperandMemoryDisplacement(ins, i);
                REG baseReg      = INS_OperandMemoryBaseReg(ins, i);
                std::cout << REG_StringShort(baseReg) << std::endl;
                std::cout << displacement << std::endl;
            }
            else if(INS_OperandIsReg(ins, i))
            {
                // register operand
                // save register
                REG src = INS_OperandReg(ins, i);
                printf("register operand\n");
                std::cout << REG_StringShort(src) << std::endl;
            }
            else if(INS_OperandIsImmediate(ins, i))
            {
                // immediate operand
                // save immediate value
                printf("immediate operand\n");
                std::cout << INS_OperandImmediate(ins, i) << std::endl;
            }
        }
        assert(false);
    }
    break;

    case XED_ICLASS_JNLE:
    {
        #if 0
        // ZF = 0 & SF = OF
        insInfo.IsBranch = true;
        insInfo.IsUnconditionalBranch = false;
        insInfo.IsConditionalBranch = true;
        insInfo.EffectsEFlags = false;
        #endif
    }
    break;

    default:
    {
        std::cerr << "Unknown opcode: " << opcode << std::endl;
        assert(false);
    }
    break;
    }

    pInsInfo->Opcode = opcode;
    pInsInfo->Addr = INS_Address(ins);
    pInsInfo->Size = INS_Size(ins);
    pInsInfo->Disassemble = INS_Disassemble(ins);
    return pInsInfo;
}


static void ImageLoad(IMG img, void *v)
{
    if (!IMG_Valid(img))
    {
        return;
    }
    
    if (IMG_IsMainExecutable(img))
    {
        s_targetName = IMG_Name(img);
    }

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {

        if (!IMG_hasLinesData(img))
        {
            // doesn't have debug info, skip section
            continue;
        }

        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            ADDRINT addr = RTN_Address(rtn);
            INT32 col = 0;
            INT32 line = 0;
            std::string filePath;
            PIN_GetSourceLocation(addr, &col, &line, &filePath);
            if (filePath == "")
            {
                // doesn't have debug info, skip function
                continue;
            }

            if (s_fileCodeCoverageMap.find(filePath) == s_fileCodeCoverageMap.end())
            {
                struct stat st;
                int ret = stat(filePath.c_str(), &st);
                if (ret != 0)
                {
                    // skip if file not exist
                    continue;
                }

                FileCodeCoverage fileCodeCoverage;
                fileCodeCoverage.FilePath = filePath;

                // read source file and save to map
                std::ifstream ifs(filePath);

                // read each line
                std::string text;
                UINT32 lineNo = 1;
                while (std::getline(ifs, text))
                {
                    LineInfo line{lineNo, text, false, false};
                    fileCodeCoverage.Lines.push_back(line);
                    lineNo++;
                }
                ifs.close();

                s_fileCodeCoverageMap[filePath] = fileCodeCoverage;

            }

            FuncCodeCoverage funcCodeCoverage;
            const std::string &funcName = RTN_Name(rtn);
            RTN_Open(rtn);

            BasicBlockInfo basicBlockInfo;
            basicBlockInfo.Executed = false;
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
            {
                addr = INS_Address(ins);
                PIN_GetSourceLocation(addr, &col, &line, &filePath);

                s_addrFuncNameMap[addr] = funcName;

                // set executable line
                // note that line number start from 1
                s_fileCodeCoverageMap[filePath].Lines[line - 1].Executable = true;

                // initialize funcCodeCoverage
                funcCodeCoverage.AddrLineMap[addr]      = line;
                funcCodeCoverage.LineCoveredMap[line]   = false;
                funcCodeCoverage.InsCoveredMap[addr]    = false;
                funcCodeCoverage.AddrAsmMap[addr]       = INS_Disassemble(ins);

                UINT32 operandCnt = INS_OperandCount(ins);
                // UINT64 v = INS_OperandImmediate(ins, 1);
                std::string disassemble = INS_Disassemble(ins);
                std::cout << StringHelper::strprintf("%s:0x%lx %s operandCount:[%d], opeElmCnt:[%d]", funcName, addr, disassemble, operandCnt) << std::endl;
                InsInfo *pInsInfo = makeInsInfo(ins);
                basicBlockInfo.InsList.push_back(pInsInfo);
                if (isBlockEnd(ins))
                {
                    funcCodeCoverage.BasicBlocks.push_back(basicBlockInfo);
                    basicBlockInfo = BasicBlockInfo();
                }
            }

            funcCodeCoverage.TotalLineCount = funcCodeCoverage.LineCoveredMap.size();
            funcCodeCoverage.CoveredLineCount = 0;
            RTN_Close(rtn);

            s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName] = funcCodeCoverage;
            s_funcFileMap[funcName] = filePath;
        }
    }

    return;
}

static VOID updateCoverage(ADDRINT addr)
{
    if (s_addrFuncNameMap.find(addr) == s_addrFuncNameMap.end())
    {
        return;
    }
    std::string funcName = s_addrFuncNameMap[addr];
    std::string filePath = s_funcFileMap[funcName];
    if (s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap.find(funcName) == s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap.end())
    {
        return;
    }

    if (s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].AddrLineMap.find(addr) == s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].AddrLineMap.end())
    {
        return;
    }

    INT32 line = s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].AddrLineMap[addr];
    s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].InsCoveredMap[addr] = true;
    if (!s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].LineCoveredMap[line])
    {
        s_fileCodeCoverageMap[filePath].Lines[line - 1].Covered = true;
        s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].LineCoveredMap[line] = true;
        s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].CoveredLineCount++;
    }
}

static std::string makeReportFileName(const std::string &filePath)
{
    // change file path to html file path
    std::string fileName = filePath;
    if (filePath.find_first_of("/") == 0)
    {
        fileName = filePath.substr(1);
    }
    fileName = std ::regex_replace(fileName, std::regex("/"), ".");
    fileName += ".html";
    return fileName;
}

static std::string makeAsmReportFileName(const std::string &filePath)
{
    // change file path to html file path
    std::string fileName = filePath;
    if (filePath.find_first_of("/") == 0)
    {
        fileName = filePath.substr(1);
    }
    fileName = "asm_" + std ::regex_replace(fileName, std::regex("/"), ".");
    fileName += ".html";
    return fileName;
}

static std::string encodeHtml(const std::string &text)
{
    std::string encodedText = text;
    encodedText = std::regex_replace(encodedText, std::regex("&"), "&amp;");
    encodedText = std::regex_replace(encodedText, std::regex("<"), "&lt;");
    encodedText = std::regex_replace(encodedText, std::regex(">"), "&gt;");
    encodedText = std::regex_replace(encodedText, std::regex("\""), "&quot;");
    encodedText = std::regex_replace(encodedText, std::regex("'"), "&apos;");
    return encodedText;
}

static void generateIndexHtml(const std::string &filePath, const std::string &targetModule)
{
    std::ofstream indexHtml(filePath);
    indexHtml << "<html><head>" << std::endl;
    indexHtml << "<meta charset='UTF-8'>" << std::endl;
    indexHtml << "<style type='text/css'>" << std::endl;
    indexHtml << ".left {" << std::endl;
    indexHtml << "    text-align: left;" << std::endl;
    indexHtml << "    padding-left: 3px;" << std::endl;
    indexHtml << "}" << std::endl;
    indexHtml << ".center {" << std::endl;
    indexHtml << "    text-align: center;" << std::endl;
    indexHtml << "}" << std::endl;
    indexHtml << ".right {" << std::endl;
    indexHtml << "    text-align: right;" << std::endl;
    indexHtml << "    padding-right: 3px;" << std::endl;
    indexHtml << "}" << std::endl;
    indexHtml << "table {" << std::endl;
    indexHtml << "    width: 100%;" << std::endl;
    indexHtml << "    border-collapse:collapse;" << std::endl;
    indexHtml << "    border: 1px #333 solid;" << std::endl;
    indexHtml << "}" << std::endl;
    indexHtml << "th {" << std::endl;
    indexHtml << "    border-collapse:collapse;" << std::endl;
    indexHtml << "    border: 1px #333 solid;" << std::endl;
    indexHtml << "    font-weight: bold;" << std::endl;
    indexHtml << "    background-color: #888;" << std::endl;
    indexHtml << "    text-align: center;" << std::endl;
    indexHtml << "    color: #EEE;" << std::endl;
    indexHtml << "}" << std::endl;
    indexHtml << "td {" << std::endl;
    indexHtml << "    border-collapse:collapse;" << std::endl;
    indexHtml << "    border: 1px #333 solid;" << std::endl;
    indexHtml << "}" << std::endl;
    indexHtml << "</style>" << std::endl;
    indexHtml << StringHelper::strprintf("<title>Code Coverage Report for %s </title>", s_targetName) << std::endl;
    indexHtml << "</head>" << std::endl;
    indexHtml << "<body>" << std::endl;
    indexHtml << StringHelper::strprintf("<h2>target module %s</h2>", targetModule) << std::endl;

    for (auto &fileCodeCoverage : s_fileCodeCoverageMap)
    {
        std::string fileName = makeReportFileName(fileCodeCoverage.first);
        indexHtml << StringHelper::strprintf("<h3><a href='%s'>%s</a></h3>", fileName, fileCodeCoverage.first) << std::endl;
        indexHtml << "<table>" << std::endl;
        indexHtml << "<thead>" << std::endl;
        indexHtml << "<tr>" << std::endl;
        indexHtml << "<th>function name</th>" << std::endl;
        indexHtml << "<th>function coverage(%)</th>" << std::endl;
        indexHtml << "<th>executed / total(lines)</th>" << std::endl;
        indexHtml << "</tr>" << std::endl;
        indexHtml << "</thead>" << std::endl;
        indexHtml << "<tbody>" << std::endl;
        for(auto &funcCodeCoverage : fileCodeCoverage.second.FuncCodeCoverageMap)
        {
            std::string funcName = funcCodeCoverage.first;
            INT32 coveredLineCount = funcCodeCoverage.second.CoveredLineCount;
            INT32 totalLineCount = funcCodeCoverage.second.TotalLineCount;
            INT32 coveredRate = 0;
            if (totalLineCount != 0)
            {
                float rate = ((float) coveredLineCount / (float)totalLineCount) * 100;
                coveredRate = std::round(rate);
            }
            indexHtml << "<tr>" << std::endl;
            indexHtml << StringHelper::strprintf("<td class='left'>%s</td>", funcName) << std::endl;
            indexHtml << StringHelper::strprintf("<td class='center'>%d%</td>", coveredRate) << std::endl;
            indexHtml << StringHelper::strprintf("<td class='center'>%d / %d</td>", coveredLineCount, totalLineCount) << std::endl;
            indexHtml << "</tr>" << std::endl;
        }
        indexHtml << "</tbody>" << std::endl;
        indexHtml << "</table>" << std::endl;
        
    }
    indexHtml << "</body></html>" << std::endl;
    indexHtml.close();
}

void generateSourceFileHtml(const std::string &reportFilePath, const std::string & filePath, const FileCodeCoverage & fileCodeCoverage)
{
    std::ofstream sourceHtml(reportFilePath);
    sourceHtml << "<html><head>" << std::endl;
    sourceHtml << "<meta charset='UTF-8'>" << std::endl;
    sourceHtml << "<style type='text/css'>" << std::endl;
    sourceHtml << "    body {" << std::endl;
    sourceHtml << "        font-size: 1rem;" << std::endl;
    sourceHtml << "        color: black;" << std::endl;
    sourceHtml << "        background-color: #EEE;" << std::endl;
    sourceHtml << "        margin-top: 0px;" << std::endl;
    sourceHtml << "        margin-bottom: 0px;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    pre {" << std::endl;
    sourceHtml << "        margin: 0px;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    table {" << std::endl;
    sourceHtml << "        width: 100%;" << std::endl;
    sourceHtml << "        border-collapse: collapse;" << std::endl;
    sourceHtml << "        border-spacing: 0px;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    td {" << std::endl;
    sourceHtml << "        margin: 0px;" << std::endl;
    sourceHtml << "        padding: 0px;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    a.link-index {" << std::endl;
    sourceHtml << "        color: #FFF;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    a.link-index:visited{" << std::endl;
    sourceHtml << "        color: #FFF;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    a.link-disassemble {" << std::endl;
    sourceHtml << "        color: #00F;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    a.link-disassemble:visited{" << std::endl;
    sourceHtml << "        color: #00F;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    .line-number {" << std::endl;
    sourceHtml << "        width: 60px;" << std::endl;
    sourceHtml << "        text-align: center;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    .code {" << std::endl;
    sourceHtml << "        text-align: left;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    .not-stmt {" << std::endl;
    sourceHtml << "        background-color: #CCC;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    .covered-line {" << std::endl;
    sourceHtml << "        background-color: #c0f7c0;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    .not-covered-line {" << std::endl;
    sourceHtml << "        background-color: #fdc8e4;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    .top-margin {" << std::endl;
    sourceHtml << "        margin-top: 15px;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "    .src-report-header {" << std::endl;
    sourceHtml << "        color: #FFF;" << std::endl;
    sourceHtml << "        font-weight: bold;" << std::endl;
    sourceHtml << "        padding-left: 10px;" << std::endl;
    sourceHtml << "        margin-top: 5px;" << std::endl;
    sourceHtml << "        margin-bottom: 5px;" << std::endl;
    sourceHtml << "        background-color: #555;" << std::endl;
    sourceHtml << "    }" << std::endl;
    sourceHtml << "</style>" << std::endl;
    sourceHtml << StringHelper::strprintf("<title>%s</title>", filePath) << std::endl;
    sourceHtml << "</head>" << std::endl;

    sourceHtml << "<body>" << std::endl;
    sourceHtml << "<div class='src-report-header'>" << std::endl;
    sourceHtml << StringHelper::strprintf("    <a href='index.html' class='link-index'>index</a> > %s", filePath) << std::endl;
    sourceHtml << "</div>" << std::endl;
    sourceHtml << "<div class='top-margin'>" << std::endl;
    sourceHtml << "<details open>" << std::endl;
    sourceHtml << "<summary>legend</summary>" << std::endl;
    sourceHtml << "<div class='covered-line'>Executed</div>" << std::endl;
    sourceHtml << "<div class='not-covered-line'>Not Executed</div>" << std::endl;
    sourceHtml << "<div class='not-stmt'>Not Stmt</div>" << std::endl;
    sourceHtml << "</details>" << std::endl;
    sourceHtml << "</div>" << std::endl;
    sourceHtml << "<div class='top-margin'>" << std::endl;
    sourceHtml << "<table cellPadding=0>" << std::endl;
    std::string asmReportFileName = makeAsmReportFileName(filePath);
    sourceHtml << StringHelper::strprintf("<h4><a href='%s' class='link-disassemble'>show disassemble</a></h4>", asmReportFileName) << std::endl;
    sourceHtml << "<tbody>" << std::endl;
    for (const auto & line : fileCodeCoverage.Lines)
    {
        if (line.Executable)
        {
            if (line.Covered)
            {
                sourceHtml << "<tr class='covered-line'>" << std::endl;
            }
            else
            {
                sourceHtml << "<tr class='not-covered-line'>" << std::endl;
            }
        }
        else
        {
            sourceHtml << "<tr class='not-stmt'>" << std::endl;
        }
        sourceHtml << "    <td class='line-number'>" << line.LineNumber << "</td>" << std::endl;
        sourceHtml << "    <td class='code'>" << std::endl;
        sourceHtml << "    <pre>" << encodeHtml(line.Text) << "</pre>" << std::endl;
        sourceHtml << "    </td>" << std::endl;
        sourceHtml << "</tr>" << std::endl;
    }
    sourceHtml << "</tbody>" << std::endl;
    sourceHtml << "</table>" << std::endl;
    sourceHtml << "</div>" << std::endl;
    sourceHtml << "</body>" << std::endl;
    sourceHtml << "</html>" << std::endl;
    sourceHtml.close();
}

void generateAsmHtml(std::string asmReportFilePath, std::string filePath, const FileCodeCoverage & fileCodeCoverage)
{
    std::ofstream asmHtml(asmReportFilePath);
    asmHtml << "<html><head>" << std::endl;
    asmHtml << "<meta charset='UTF-8'>" << std::endl;
    asmHtml << "<style type='text/css'>" << std::endl;
    asmHtml << "    body {" << std::endl;
    asmHtml << "        font-size: 1rem;" << std::endl;
    asmHtml << "        color: black;" << std::endl;
    asmHtml << "        background-color: #EEE;" << std::endl;
    asmHtml << "        margin-top: 0px;" << std::endl;
    asmHtml << "        margin-bottom: 0px;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    pre {" << std::endl;
    asmHtml << "        margin: 0px;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    table {" << std::endl;
    asmHtml << "        width: 100%;" << std::endl;
    asmHtml << "        border-collapse: collapse;" << std::endl;
    asmHtml << "        border-spacing: 0px;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    td {" << std::endl;
    asmHtml << "        margin: 0px;" << std::endl;
    asmHtml << "        padding: 0px;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    a.link-index {" << std::endl;
    asmHtml << "        color: #FFF;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    a.link-index:visited{" << std::endl;
    asmHtml << "        color: #FFF;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    a.link-report {" << std::endl;
    asmHtml << "        color: #00F;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    a.link-report:visited{" << std::endl;
    asmHtml << "        color: #00F;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    .line-number {" << std::endl;
    asmHtml << "        width: 60px;" << std::endl;
    asmHtml << "        text-align: center;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    .code {" << std::endl;
    asmHtml << "        text-align: left;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    .ins-addr {" << std::endl;
    asmHtml << "        width: 60px;" << std::endl;
    asmHtml << "        text-align: center;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    .mnemonic {" << std::endl;
    asmHtml << "        text-align: left;" << std::endl;
    asmHtml << "        padding-left: 1.5em;" << std::endl;
    asmHtml << "        width: 30%;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    .not-stmt {" << std::endl;
    asmHtml << "        background-color: #CCC;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    .covered-line {" << std::endl;
    asmHtml << "        background-color: #c0f7c0;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    .not-covered-line {" << std::endl;
    asmHtml << "        background-color: #fdc8e4;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    .top-margin {" << std::endl;
    asmHtml << "        margin-top: 15px;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "    .src-report-header {" << std::endl;
    asmHtml << "        color: #FFF;" << std::endl;
    asmHtml << "        font-weight: bold;" << std::endl;
    asmHtml << "        padding-left: 10px;" << std::endl;
    asmHtml << "        margin-top: 5px;" << std::endl;
    asmHtml << "        margin-bottom: 5px;" << std::endl;
    asmHtml << "        background-color: #555;" << std::endl;
    asmHtml << "    }" << std::endl;
    asmHtml << "</style>" << std::endl;
    asmHtml << StringHelper::strprintf("<title>%s</title>", filePath) << std::endl;
    asmHtml << "</head>" << std::endl;

    asmHtml << "<body>" << std::endl;
    asmHtml << "<div class='src-report-header'>" << std::endl;
    asmHtml << StringHelper::strprintf("    <a href='index.html' class='link-index'>index</a> > %s", filePath) << std::endl;
    asmHtml << "</div>" << std::endl;
    asmHtml << "<div class='top-margin'>" << std::endl;
    asmHtml << "<details open>" << std::endl;
    asmHtml << "<summary>legend</summary>" << std::endl;
    asmHtml << "<div class='covered-line'>Executed</div>" << std::endl;
    asmHtml << "<div class='not-covered-line'>Not Executed</div>" << std::endl;
    asmHtml << "<div class='not-stmt'>Not Stmt</div>" << std::endl;
    asmHtml << "</details>" << std::endl;
    asmHtml << "</div>" << std::endl;
    asmHtml << "<div class='top-margin'>" << std::endl;
    std::string reportFileName = makeReportFileName(filePath);
    asmHtml << StringHelper::strprintf("<h4><a href='%s' class='link-report'>Show sorce file</a></h4>", reportFileName) << std::endl;
    asmHtml << "<div class='top-margin'>" << std::endl;
    INT32 prevLineNo = -1;
    for (const auto & funcEntry : fileCodeCoverage.FuncCodeCoverageMap)
    {
        std::string funcName = funcEntry.first;
        asmHtml << "<table cellPadding=0>" << std::endl;
        asmHtml << "<h4>Function Name: " << funcName << "</h4>" << std::endl;
        asmHtml << "<tbody>" << std::endl;
        FuncCodeCoverage funcCodeCoverage = funcEntry.second;
        for (const auto & addrEntry : funcCodeCoverage.AddrAsmMap)
        {
            ADDRINT addr = addrEntry.first;
            std::string mnemonic = addrEntry.second;
            if (funcCodeCoverage.InsCoveredMap[addr])
            {
                asmHtml << "<tr class='covered-line'>" << std::endl;
            }
            else
            {
                asmHtml << "<tr class='not-covered-line'>" << std::endl;
            }
            asmHtml << "    <td class='ins-addr'>" << "0x" << std::hex << addr << "</td>" << std::endl;
            asmHtml << "    <td class='mnemonic'>" << std::endl;
            asmHtml << "    <pre>" << encodeHtml(mnemonic) << "</pre>" << std::endl;
            asmHtml << "    </td>" << std::endl;

            bool showLine = false;
            if (funcCodeCoverage.AddrLineMap.find(addr) != funcCodeCoverage.AddrLineMap.end())
            {
                INT32 lineNo = funcCodeCoverage.AddrLineMap[addr];
                if (prevLineNo != lineNo)
                {
                    showLine = true;
                    prevLineNo = lineNo;
                    asmHtml << "    <td class='line-number'>";
                    asmHtml << std::dec << lineNo << "</td>" << std::endl;
                    asmHtml << "    <td class='code'>" << fileCodeCoverage.Lines[lineNo - 1].Text << "</td>" << std::endl;
                }
            }
            if (!showLine)
            {
                asmHtml << "    <td class='line-number'></td>" << std::endl;
                asmHtml << "    <td class='code'></td>" << std::endl;
            }
            asmHtml << "</tr>" << std::endl;
        }
        asmHtml << "</tbody>" << std::endl;
        asmHtml << "</table>" << std::endl;
    }

    asmHtml << "</div>" << std::endl;
    asmHtml << "</body>" << std::endl;
    asmHtml << "</html>" << std::endl;
    asmHtml.close();
}

static VOID analyzeResult(VOID)
{
#if 0
    // generate each source file html
    for (auto &entry : s_fileCodeCoverageMap)
    {
        std::string sourceFilePath = entry.first;
        FileCodeCoverage fileCodeCoverage = entry.second;
        
        for (auto &funcEntry : fileCodeCoverage.FuncCodeCoverageMap)
        {
            std::string funcName = funcEntry.first;
            FuncCodeCoverage funcCodeCoverage = funcEntry.second;
            for (auto &basicBlock : funcCodeCoverage.BasicBlocks)
            {
                for (auto &ins : basicBlock.InsList)
                {
                    ADDRINT addr = INS_Address(ins);
                    std::string nemonic = INS_Mnemonic(ins);
                    std::string disassemble = INS_Disassemble(ins);
                    std::cout << StringHelper::strprintf("%s:0x%lx %s %s", funcName, addr, nemonic, disassemble) << std::endl;
                }
            break;
            }
        }
    }
#endif
    return;
}

static VOID Instruction(INS ins, VOID *v)
{
    ADDRINT addr = INS_Address(ins);
    std::string funcName = RTN_FindNameByAddress(addr);
    if (funcName == "")
    {
        return;
    }

    UINT32 operandCnt = INS_OperandCount(ins);
    // UINT64 v = INS_OperandImmediate(ins, 1);
    std::string disassemble = INS_Disassemble(ins);
    std::cout << StringHelper::strprintf("%s:0x%lx %s operandCount:[%d], opeElmCnt:[%d]", funcName, addr, disassemble, operandCnt) << std::endl;
    for (UINT32 op = 0; op < INS_OperandCount(ins); op++)
    {
        if (INS_OperandIsImplicit(ins,op))
        {
            std::cerr << "Implicit: " << std::endl;
        }
        if (INS_OperandIsReg(ins,op))
        {
            std::cerr << "IsReg: " << std::endl;
        }
        
        if (INS_OperandIsImmediate(ins, op))
        {
            std::cerr << "Immediate: " << std::hex << INS_OperandImmediate(ins, op) << std::endl;
        }
        if (INS_OperandIsMemory(ins, op))
        {
            std::cerr << "Memory: " << std::endl;
        }
    }

    if (s_funcFileMap.find(funcName) == s_funcFileMap.end())
    {
        return;
    }

    std::string filePath = s_funcFileMap[funcName];
    if (s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap.find(funcName) == s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap.end())
    {
        return;
    }

    if (s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].AddrLineMap.find(addr) == s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].AddrLineMap.end())
    {
        return;
    }

    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)updateCoverage, IARG_ADDRINT, addr, IARG_END);
}

VOID Fini(INT32 code, VOID* v)
{
    std::cout << "[CodeCoverage] Program trace Finished, generating Coverage report..." << std::endl;

    analyzeResult();

    // generate index.html
    struct stat st;
    int ret = stat("report", &st);
    if (ret < 0)
    {
        // create report dir if not exist
        mkdir("report", 0755);
    }

    generateIndexHtml("report/index.html", s_targetName);

    // generate each source file html
    for (auto &entry : s_fileCodeCoverageMap)
    {
        std::string sourceFilePath = entry.first;
        FileCodeCoverage &fileCodeCoverage = entry.second;
        std::string reportFilePath    = "report/" + makeReportFileName(sourceFilePath);
        std::string asmReportFilePath = "report/" + makeAsmReportFileName(sourceFilePath);
        generateSourceFileHtml(reportFilePath, sourceFilePath, fileCodeCoverage);
        generateAsmHtml(asmReportFilePath, sourceFilePath, fileCodeCoverage);
    }

    std::cout << "[CodeCoverage] Coverage Report generated. Please check `report/index.html' using your browser." << std::endl;
    return;
}

int main(int argc, char **argv)
{
    std::cout << "[CodeCoverage] Start..." << std::endl;
    PIN_InitSymbols();
    if(PIN_Init(argc,argv)) {
        std::exit(EXIT_FAILURE);
    }

    IMG_AddInstrumentFunction(ImageLoad, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    std::cout << "[CodeCoverage] Program trace Start" << std::endl;

    PIN_StartProgram();
    std::exit(EXIT_SUCCESS);
}
