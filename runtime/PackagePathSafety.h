#pragma once
#include <Windows.h>
#include <filesystem>
#include <string>
#include <stdexcept>
namespace DanceStudio {
inline std::wstring FinalPackagePath(const std::filesystem::path&p,bool directory){
 HANDLE h=CreateFileW(p.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|(directory?FILE_FLAG_BACKUP_SEMANTICS:0),nullptr);
 if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot inspect package path");
 struct Close{HANDLE h;~Close(){CloseHandle(h);}}close{h};FILE_ATTRIBUTE_TAG_INFO info{};
 if(!GetFileInformationByHandleEx(h,FileAttributeTagInfo,&info,sizeof(info))||(info.FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Package reparse point rejected");
 std::wstring path(32768,L'\0');DWORD n=GetFinalPathNameByHandleW(h,path.data(),static_cast<DWORD>(path.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);if(!n||n>=path.size())throw std::runtime_error("Cannot resolve package path");path.resize(n);return path;
}
inline void ValidatePackagePath(const std::filesystem::path&root,const std::filesystem::path&relative){
 auto current=root;auto rootPhysical=FinalPackagePath(root,true);
 for(auto it=relative.begin();it!=relative.end();++it){auto name=it->wstring();if(name.empty()||name==L".")continue;if(name.back()==L'.'||name.back()==L' '||name.find(L':')!=name.npos||name.find(L'\0')!=name.npos)throw std::runtime_error("Invalid Windows package component");current/=*it;auto attributes=GetFileAttributesW(current.c_str());if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Unverifiable/reparse package component");FinalPackagePath(current,(attributes&FILE_ATTRIBUTE_DIRECTORY)!=0);}
 // USVFS deliberately merges several physical mod roots. Never equate that
 // legitimate mapping with a package escape. Reparse components remain denied.
 if(!GetModuleHandleW(L"usvfs_x64.dll")&&!GetModuleHandleW(L"usvfs_x86.dll")){
  auto final=FinalPackagePath(current,false);rootPhysical+=L'\\';if(final.size()<=rootPhysical.size()||_wcsnicmp(final.c_str(),rootPhysical.c_str(),rootPhysical.size())!=0)throw std::runtime_error("Physical package escape rejected");
 }
}
}
