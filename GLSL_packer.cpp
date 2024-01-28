#include <windows.h>

static int
LengthOfString(char *String)
{
    int Length = 0;
    while(*(String++))
    {
        Length++;
    }
    return(Length);
}

static bool
DisplayError(char *Title)
{
    DWORD ErrorMessageID = GetLastError();
    if(ErrorMessageID == 0) {
        return(false);
    }
    
    DWORD BytesWritten = 0;
    HANDLE ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    
    LPSTR messageBuffer = 0;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, ErrorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    
    WriteFile(ConsoleHandle, Title, LengthOfString(Title), &BytesWritten, 0);
    
    char *ErrorMessageSuffix = " caused an error:\n";
    WriteFile(ConsoleHandle, ErrorMessageSuffix, LengthOfString(ErrorMessageSuffix), &BytesWritten, 0);
    
    WriteFile(ConsoleHandle, messageBuffer, LengthOfString(messageBuffer), &BytesWritten, 0);
    
    char *LineBreak = "\n";
    WriteFile(ConsoleHandle, LineBreak, LengthOfString(LineBreak), &BytesWritten, 0);
    
    return(true);
}

int CALLBACK
WinMain(HINSTANCE Instance,
		HINSTANCE PrevInstance,
		LPSTR     CommandLine, 
		int       ShowCode)
{
    char Path[MAX_PATH];
    char *PathEnd = Path + MAX_PATH;
    DWORD NameLength = GetModuleFileNameA(0, Path, sizeof(Path));
    char *End = Path + NameLength;
    char *At = End;
    while(At >= Path && *At != '/' && *At != '\\')
    {
        *(At--) = 0;
    }
    char *CurrentDirectoryCharacter = ++At;
    
    char *OutputFile = "shader_globals.cpp";
    At = CurrentDirectoryCharacter;
    char *From = OutputFile;
    while(*From)
    {
        *At++ = *From++;
    }
    *At = 0;
    
    HANDLE OutputFileHandle = CreateFileA(Path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    
    if(OutputFileHandle == INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten = 0;
        char *ErrorMessage = "Failed to create glsl_shader_globals.cpp.\n";
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), ErrorMessage, LengthOfString(ErrorMessage), &BytesWritten, 0);
        return(0);
    }
    
    At = CurrentDirectoryCharacter;
    *At++ = '*';
    *At = 0;
    
    WIN32_FIND_DATA SearchResult;
    HANDLE FindHandle = INVALID_HANDLE_VALUE;
    
    FindHandle = FindFirstFileA(Path, &SearchResult);
    if(FindHandle == INVALID_HANDLE_VALUE) 
    {
        DWORD BytesWritten = 0;
        char *ErrorMessage = "Failed to load shader directory.\n";
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), ErrorMessage, LengthOfString(ErrorMessage), &BytesWritten, 0);
        return(0);
    }
    
    do
    {
        if(!(SearchResult.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            DWORD BytesWritten = 0;
            
            At = CurrentDirectoryCharacter;
            From = SearchResult.cFileName;
            int FileNameLength = 0;
            while(*From)
            {
                *(At++) = *(From++);
                FileNameLength++;
            }
            while(At < PathEnd)
            {
                *(At++) = 0;
            }
            From++;
            
            char *Suffix = ".glsl";
            At = Suffix + LengthOfString(Suffix) + 1;
            bool SuffixMatches = true;
            while(At > Suffix)
            {
                SuffixMatches &= *(--At) == *(--From);
                
#if 0
                WriteFile(OutputFileHandle, At, 1, &BytesWritten, 0);
                WriteFile(OutputFileHandle, From, 1, &BytesWritten, 0);
#endif
            }
            
            if(SuffixMatches)
            {
                HANDLE ReadFileHandle = CreateFileA(Path, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
                LARGE_INTEGER FileSize;
                GetFileSizeEx(ReadFileHandle, &FileSize);
                if(DisplayError(SearchResult.cFileName))
                    return(0);
                
                if(FileSize.HighPart)
                {
                    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), SearchResult.cFileName, LengthOfString(SearchResult.cFileName), &BytesWritten, 0);
                    char *ErrorMessage = " is too large.\n";
                    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), ErrorMessage, LengthOfString(ErrorMessage), &BytesWritten, 0);
                    return(0);
                }
                
                char *VariablePrefix = "static char *";
                WriteFile(OutputFileHandle, VariablePrefix, LengthOfString(VariablePrefix), &BytesWritten, 0);
                WriteFile(OutputFileHandle, SearchResult.cFileName, FileNameLength - LengthOfString(Suffix), &BytesWritten, 0);
                char *VariableSuffix = "Code = R\"glsl(\r\n";
                WriteFile(OutputFileHandle, VariableSuffix, LengthOfString(VariableSuffix), &BytesWritten, 0);
                
                void *FileMemory = VirtualAlloc(0, FileSize.LowPart, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
                DWORD BytesRead = 0;
                
                ReadFile(ReadFileHandle, FileMemory, FileSize.LowPart, &BytesRead, 0);
                if(DisplayError(SearchResult.cFileName))
                    return(0);
                
                WriteFile(OutputFileHandle, FileMemory, BytesRead, &BytesWritten, 0);
                
                VirtualFree(FileMemory, 0, MEM_RELEASE);
                
                char *CodeSuffix = "\r\n)glsl\";\r\n\r\n";
                WriteFile(OutputFileHandle, CodeSuffix, LengthOfString(CodeSuffix), &BytesWritten, 0);
            }
        }
    }
    while(FindNextFile(FindHandle, &SearchResult) != 0);
    
    DWORD BytesWritten = 0;
    char *SuccessMessage = "Successfully conveted all GLSL files to shader_globals.cpp.\n\n";
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), SuccessMessage, LengthOfString(SuccessMessage), &BytesWritten, 0);
    return(0);
}