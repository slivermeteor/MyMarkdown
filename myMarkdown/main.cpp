#include <iostream>
#include <fstream>
#include <Windows.h>

#include "FileOperate.h"

namespace start
{
    class COptions 
    {  
        private:
        std::string m_ParseFileFullPath; 
		std::string m_OutputFileFullPath;

        public:
        COptions()
        {
        }
        ~COptions()
        {
            
        }

        bool ReadOptions(int argc, char* argv[]);
        void ShowHelp();

        const std::string ParseFileFullPath()
        {
            return m_ParseFileFullPath;
        }    
		const std::string OutputFileFullPath()
		{
			return m_OutputFileFullPath;
		}
	};

    bool COptions::ReadOptions(int argc, char* argv[])
    {
		// 检查所有的命令行参数
        for (int i = 1; i < argc; i++)
        {
            // markdown.exe -d (fullpath)
            if (argv[i][0] == '-' && argv[i][1] == 'h') // -h 优先判断，并且忽略后面所有参数
            {
				ShowHelp();
				return true;;
            }
			else if (argv[i][0] == '-' && argv[i][1] == 'o')
			{
				if (m_OutputFileFullPath.empty())
				{
					m_OutputFileFullPath = &argv[i][3];
				}
				else
				{
					std::cout << "There are too many parameters.Please input only one out file path." << std::endl;
					return false;
				}
			}
            else if (argv[i][0] == '-' && argv[i][1] == 'p')
            {
				
				if (m_ParseFileFullPath.empty())
				{
					m_ParseFileFullPath = &argv[i][3];
				}
				else
				{
					std::cout << "There are too many parameters.Please input only one in file path." << std::endl;
					return false;
				}
            }
			else if (argv[i][0] == '-')
			{
				std::cout << "Invalid options.Pleast use -h to see how to use this program." << std::endl;
				return false;
			}
        }

		// 检查两个路径是否输入
		if (m_ParseFileFullPath.empty())
		{
			std::cout << "Please input the convert file full path use -p option." << std::endl;
			return false;
		}
		if (m_OutputFileFullPath.empty())
		{
			std::cout << "Please input the out file full path use -o option." << std::endl;
			return false;
		}

		return true;
    }

    void COptions::ShowHelp()
    {
        const char* Help = 
        "This program help you converting a file what wroten by markdown syntax to a html file.\r\n\
        \r\n\
        Usage myMarkdown.exe [<options>][file full path]\r\n\
        \r\n\
		-p, infilepath    please input in file full path after this option.\r\n\
		-o, outfilepath   please input out file full path after this option.\r\n\
		-h, help          show the help of this program.\r\n";

        std::cout<<std::endl<<Help<<std::endl;
    }

}



int main(int argc, char* argv[])
{
	unsigned long a = 1u;
	using namespace start;
	COptions Options;
	if (!Options.ReadOptions(argc, argv))
	{
		return -1;
	}

	std::ifstream InFile;
	std::ofstream OutFile;
	std::cout << "Start reading file:" << Options.ParseFileFullPath().c_str() << std::endl;
	InFile.open(Options.ParseFileFullPath().c_str());
	if (!InFile)
	{
		std::cout << "Open input file failed with a error code:" << GetLastError() << std::endl;
		return -1;
	}
    
	Markdown::CFileOperate FileOperate;
	if (!FileOperate.ReadFile(InFile))
	{
		std::cout << "Read input file failed." << std::endl;
		return -1;
	}
	
	OutFile.open(Options.OutputFileFullPath().c_str());
	if (!OutFile)
	{
		std::cout << "Open output file failed with a error code:" << GetLastError() << std::endl;
		return -1;
	}
	if (!FileOperate.WriteFile(OutFile))
	{
		std::cout << "Write output file failed." << std::endl;
		return -1;
	}

    system("pause");
    return 0;
}