#include <cmath>
#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <regex>

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

static void initializeCovorage(IMG img, void *v)
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
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
            {
                addr = INS_Address(ins);
                PIN_GetSourceLocation(addr, &col, &line, &filePath);

                // set executable line
                // note that line number start from 1
                s_fileCodeCoverageMap[filePath].Lines[line - 1].Executable = true;

                // initialize funcCodeCoverage
                funcCodeCoverage.AddrLineMap[addr] = line;
                funcCodeCoverage.LineCoveredMap[line] = false;
                funcCodeCoverage.InsCoveredMap[addr] = false;
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
        s_fileCodeCoverageMap[filePath].FuncCodeCoverageMap[funcName].CoveredLineCount++;
    }
}

static std::string makeReportFileName(std::string filePath)
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

void generateSourceFileHtml(std::string reportFilePath, std::string filePath, const FileCodeCoverage & fileCodeCoverage)
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
    sourceHtml << StringHelper::strprintf("<title>%s</title>", filePath) << std::endl;
    sourceHtml << "</head>" << std::endl;

    sourceHtml << "<body>" << std::endl;
    sourceHtml << "<div class='src-report-header'>" << std::endl;
    sourceHtml << StringHelper::strprintf("    <a href='index.html'>index</a> > %s", filePath) << std::endl;
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

VOID generateCoverageReport(INT32 code, VOID* v)
{
    std::cout << "[CodeCoverage] Program trace Finished, generating Coverage report..." << std::endl;

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
        std::string reportFileName = makeReportFileName(sourceFilePath);
        std::string reportFilePath = "report/" + reportFileName;
        generateSourceFileHtml(reportFilePath, sourceFilePath, fileCodeCoverage);
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

    IMG_AddInstrumentFunction(initializeCovorage, NULL);
    INS_AddInstrumentFunction(updateCoverage, NULL);
    PIN_AddFiniFunction(generateCoverageReport, 0);

    std::cout << "[CodeCoverage] Program trace Start" << std::endl;

    PIN_StartProgram();
    std::exit(EXIT_SUCCESS);
}
