#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include "pin.H"
#include "util.h"

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

struct FuncCodeCoverage
{
    std::string Name;
    std::map<ADDRINT, INT32> AddrLineMap;
    std::map<INT32, bool> LineCoveredMap;
    std::map<ADDRINT, bool> InsCoveredMap;
    UINT32 TotalLines;
    UINT32 CoveredLines;
};

struct FileCodeCoverage
{
    std::string Name;
    std::map<std::string, FuncCodeCoverage> FuncCodeCoverageMap;
    std::vector<LineInfo> Lines;
};

// =====================================================================
// Global Variables
// =====================================================================
static std::map<std::string, FileCodeCoverage> s_fileCodeCoverageMap;
static std::map<std::string, std::string> s_funcFileMap;

static void initializeCovorage(IMG img, void *v)
{
    s_fileCodeCoverageMap.clear();

    if (!IMG_Valid(img))
    {
        return;
    }

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        std::string imgName = IMG_Name(img);
        std::cout << "imgName:" << imgName << std::endl;

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
            std::string filePath = "";
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

                // read source file and save to map
                std::ifstream ifs(filePath);

                // read each line
                std::string text;
                UINT32 lineNo = 1;
                while (std::getline(ifs, text))
                {
                    LineInfo line{lineNo, text, false, false};
                    s_fileCodeCoverageMap[filePath].Lines.push_back(line);
                    lineNo++;
                }
                ifs.close();

                // initialize fileCodeCoverage
                FileCodeCoverage fileCodeCoverage;
                fileCodeCoverage.Name = filePath;
                s_fileCodeCoverageMap[filePath] = fileCodeCoverage;
            }

            FuncCodeCoverage funcCodeCoverage;
            const std::string &funcName = RTN_Name(rtn);

            RTN_Open(rtn);
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
            {
                addr = INS_Address(ins);
                PIN_GetSourceLocation(addr, &col, &line, &filePath);

                // initialize funcCodeCoverage
                funcCodeCoverage.AddrLineMap[addr] = line;
                funcCodeCoverage.LineCoveredMap[line] = false;
                funcCodeCoverage.InsCoveredMap[addr] = false;
            }

            // set executable line
            // note that line number start from 1
            s_fileCodeCoverageMap[filePath].Lines[line - 1].Executable = true;
            funcCodeCoverage.TotalLines = funcCodeCoverage.LineCoveredMap.size();
            funcCodeCoverage.CoveredLines = 0;
            RTN_Close(rtn);

            s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName] = funcCodeCoverage;
            s_funcFileMap[funcName] = filePath;
        }
    }

#if 0
    for (auto &fileCodeCoverage : s_fileCodeCoverageMap)
    {
        std::cout << "file: " << fileCodeCoverage.first << std::endl;
        for (auto &funcCodeCoverage : fileCodeCoverage.second.FuncCodeCoverageMap)
        {
            std::cout << "func: " << funcCodeCoverage.first << std::endl;
            for (auto &lineCovered : funcCodeCoverage.second.LineCoveredMap)
            {
                std::cout << "line: " << lineCovered.first << std::endl;
            }
        }
    }
#endif

    return;
}

VOID updateCoverage(INS ins, VOID* v)
{
    ADDRINT addr = INS_Address(ins);
    std::string funcName = RTN_FindNameByAddress(addr);
    if (funcName == "")
    {
        return;
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

    INT32 line = s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].AddrLineMap[addr];
    s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].InsCoveredMap[addr] = true;
    if (!s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].LineCoveredMap[line])
    {
        s_fileCodeCoverageMap[filePath].Lines[line - 1].Covered = true;
        s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].LineCoveredMap[line] = true;
        s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].CoveredLines++;
    }
}

void generateIndexHtml(std::string filePath)
{
    // TODO 
    std::string targetModule;
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
    indexHtml << "<title>Code Coverage Report</title>" << std::endl;
    indexHtml << "</head>" << std::endl;
    indexHtml << "<body>" << std::endl;
    indexHtml << StringHelper::strprintf("<h2>target module %s</h2>", targetModule) << std::endl;

    // TODO
    std::string fileName = "";
    std::string fileHtml = fileName + ".html";
    std::string ReportFileLink = StringHelper::strprintf("<a href='%s'>%s</a>", fileHtml, fileName);
    indexHtml << StringHelper::strprintf("<h3><a href='%s'>%s</a></h3>", ReportFileLink, fileHtml) << std::endl;
    indexHtml << "<table>" << std::endl;
    indexHtml << "<thead>" << std::endl;
    indexHtml << "<tr>" << std::endl;
    indexHtml << "<th>function name</th>" << std::endl;
    indexHtml << "<th>function coverage(%)</th>" << std::endl;
    indexHtml << "<th>executed / total(lines)</th>" << std::endl;
    indexHtml << "</tr>" << std::endl;
    indexHtml << "</thead>" << std::endl;
    indexHtml << "<tbody>" << std::endl;
    for (auto &fileCodeCoverage : s_fileCodeCoverageMap)
    {
        for(auto &funcCodeCoverage : fileCodeCoverage.second.FuncCodeCoverageMap)
        {
            std::string funcName = funcCodeCoverage.first;
            INT32 totalLineCount = funcCodeCoverage.second.TotalLines;
            INT32 coveredRate = 0;
            if (totalLineCount != 0)
            {
                coveredRate = funcCodeCoverage.second.CoveredLines * 100 / totalLineCount;
            }
            INT32 coveredLineCount = 0;
            indexHtml << "<tr>" << std::endl;
            indexHtml << StringHelper::strprintf("<td class='left'>%s</td>", funcName) << std::endl;
            indexHtml << StringHelper::strprintf("<td class='center'>%d</td>", coveredRate) << std::endl;
            indexHtml << StringHelper::strprintf("<td class='center'>%d / %d</td>", coveredLineCount, totalLineCount) << std::endl;
            indexHtml << "</tr>" << std::endl;
        }
    }
    indexHtml << "</tbody>" << std::endl;
    indexHtml << "</body></html>" << std::endl;
    indexHtml.close();
}

void generateSourceFileHtml(std::string filePath, const FileCodeCoverage & fileCodeCoverage)
{
    std::string fileHtml = filePath + ".html";
    std::ofstream sourceHtml(fileHtml);
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
    sourceHtml << "    a {" << std::endl;
    sourceHtml << "        color: #fff;" << std::endl;
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

    sourceHtml << "<title>Code Coverage Report</title>" << std::endl;
    sourceHtml << "</head>" << std::endl;
    sourceHtml << "<body>" << std::endl;
    sourceHtml << "<div class='src-report-header'>" << std::endl;
    sourceHtml << "    <a href={{ .IndexRelPath }}>index</a> > {{ .FileCovInfo.Filepath }}" << std::endl;
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
    sourceHtml << "<h4>code coverage detail</h4>" << std::endl;
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
        sourceHtml << "    <pre>" <<  line.Text << "</pre>" << std::endl;
        sourceHtml << "    </td>" << std::endl;
        sourceHtml << "</tr>" << std::endl;
    }
    sourceHtml << "</tbody>" << std::endl;
    sourceHtml << "</table>" << std::endl;
    sourceHtml << "</div>" << std::endl;
    sourceHtml << "</body>" << std::endl;
    sourceHtml.close();
}

VOID generateCoverageReport(INT32 code, VOID* v)
{
    // generate index.html
    struct stat st;
    int ret = stat("report", &st);
    if (ret < 0)
    {
        // create report dir if not exist
        mkdir("report", 0755);
    }

    generateIndexHtml("report/index.html");
    
    // generate each source file html
    for (auto &fileCodeCoverage : s_fileCodeCoverageMap)
    {
        generateSourceFileHtml(fileCodeCoverage.first, fileCodeCoverage.second);
    }

    return;
}

int main(int argc, char **argv)
{
    PIN_InitSymbols();
    if(PIN_Init(argc,argv)) {
        std::exit(EXIT_FAILURE);
    }

    IMG_AddInstrumentFunction(initializeCovorage, NULL);
    INS_AddInstrumentFunction(updateCoverage, NULL);
    PIN_AddFiniFunction(generateCoverageReport, 0);

    PIN_StartProgram();
    std::exit(EXIT_SUCCESS);
}
