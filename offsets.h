#pragma once
#include "sigscanner.h"

struct Offset
{
    std::string moduleName;
    uintptr_t offset;
    uintptr_t address;
    std::string signature;
    int sigOffset;

	Offset(){};

    Offset(std::string moduleName, uintptr_t currentOffset, std::string signature, int sigOffset = 0)
    {
        this->moduleName = moduleName;
        this->offset = currentOffset;
        this->signature = signature;
        this->sigOffset = sigOffset;

        uintptr_t newOffset = SigScanner::VerifyOffset(moduleName, currentOffset, signature, sigOffset);
        if (newOffset > 0)
        {
            this->offset = newOffset;
        }

        if (newOffset == -1)
        {
            OutputDebugStringA(("Signature not found: " + signature).c_str());
            return;
        }

        this->address = (uintptr_t)GetModuleHandle(moduleName.c_str()) + this->offset;
    }
};

class Offsets
{
public:
	Offsets()
	{
		CreateEntityByName = Offset("client.dll", 0x80f10, "40 53 48 83 EC 20 48 8B D9 E8 ? ? ? ? 48 8B D3 48 8B C8 4C 8B 00 41 FF 50 18 48 85 C0 75 12 48 8B D3 48 8D 0D ? ? ? ? FF 15 ? ? ? ? 33 C0 48 83 C4 20 5B C3");
		SetModelPointer = Offset("client.dll", 0x87b60, "48 89 5C 24 10 57 48 83 EC 20 48 8B FA 48 8B D9 48 3B 91 98 00 00 00");
		FollowEntity = Offset("client.dll", 0x4ac60, "48 89 5C 24 ? 57 48 83 EC 20 41 0F B6 F8 48 8B D9 48 85 D2 74 5C 45 33 C0");
		SendViewModelMatchingSequence = Offset("client.dll", 0x51aa0, "40 53 48 83 EC 20 48 8B 01 48 8B D9 FF 90 E8 05 00 00");
		BuildTransformations = Offset("client.dll", 0x6a570, "48 8B C4 4C 89 48 20 4C 89 40 18 55 53 41 57");
		GetBonePosition = Offset("client.dll", 0x6f0f0, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 50 8B DA");
		SetAbsOrigin = Offset("client.dll", 0x87510, "48 89 5C 24 10 57 48 83 EC 20 48 8B FA 48 8B D9 E8 ? ? ? ? F3 0F 10 83 08 03 00 00");
		RenderView = Offset("client.dll", 0x1c84a0, "48 8B C4 44 89 48 20 44 89 40 18 48 89 50 10 48 89 48 08 55 53");
		CreateMove = 0x632bd0;
	}

    Offset CreateEntityByName;
    Offset SetModelPointer;
    Offset FollowEntity;
    Offset SendViewModelMatchingSequence;
    Offset BuildTransformations;
    Offset GetBonePosition;
    Offset SetAbsOrigin;
	Offset RenderView;

	uintptr_t CreateMove;
};
