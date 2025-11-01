#pragma once
#include <Windows.h>
#include <psapi.h>
#include <vector>
#include <sstream>
#include "stdint.h"

class SigScanner
{
public:
	// Returns 0 if current offset matches, -1 if no matches found.
	// A value > 0 is the new offset.
	static uintptr_t VerifyOffset( std::string moduleName, uintptr_t currentOffset, std::string signature, int sigOffset = 0 )
	{
		HMODULE hModule = GetModuleHandle( moduleName.c_str() );
		if( hModule == 0 )
			return -1;

		MODULEINFO moduleInfo;
		GetModuleInformation( GetCurrentProcess(), hModule, &moduleInfo, sizeof( moduleInfo ) );

		uint8_t *bytes = (uint8_t *)moduleInfo.lpBaseOfDll;

		std::vector<int> pattern;

		std::stringstream ss( signature );
		std::string sigByte;
		while( ss >> sigByte )
		{
			if( sigByte == "?" || sigByte == "??" )
				pattern.push_back( -1 );
			else
				pattern.push_back( strtoul( sigByte.c_str(), NULL, 16 ) );
		}

		size_t patternLen = pattern.size();

		// Check if current offset is good
		bool offsetMatchesSig = true;
		for( size_t i = 0; i < patternLen; ++i )
		{
			if( ( bytes[currentOffset - sigOffset + i] != pattern[i] ) && ( pattern[i] != -1 ) )
			{
				offsetMatchesSig = false;
				OutputDebugStringA("CALLL offsetMatchesSig FALSE\n");
				break;
			}
		}

		if( offsetMatchesSig )
			return 0;

		// Scan the dll for new offset
		for( uintptr_t i = 0; i < static_cast<uintptr_t>( moduleInfo.SizeOfImage ); ++i )
		{
			bool found = true;
			for( int j = 0; j < patternLen; ++j )
			{
				if( ( bytes[i + j] != pattern[j] ) && ( pattern[j] != -1 ) )
				{
					found = false;
					break;
				}
			}
			if( found )
			{
				OutputDebugStringA("CALLL found found found\n");
				return i + sigOffset;
			}
		}
		return -1;
	}
};