#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include "pin.H"
#include "util.h"

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
    std::ifstream ifs("templates/index_head.tpl");
    std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    indexHtml << s;
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
    std::ofstream sourceFileHtml(fileHtml);
    sourceFileHtml << "<html><head>" << std::endl;
    std::ifstream ifs("templates/source_head.tpl");
    std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    sourceFileHtml << s;
    sourceFileHtml << "<title>Code Coverage Report</title>" << std::endl;
    sourceFileHtml << "</head>" << std::endl;
    sourceFileHtml << "<body>" << std::endl;

    // TODO
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
