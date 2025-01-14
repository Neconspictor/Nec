/*////////////////////////////////////////////////////////////////////////////

This file is part of neclib.

Copyright � 2015-2020 David Goeth

All Rights reserved.

THE WORK (AS DEFINED BELOW) IS PROVIDED
UNDER THE TERMS OF THIS CREATIVE COMMONS
PUBLIC LICENSE ("CCPL" OR "LICENSE").
THE WORK IS PROTECTED BY COPYRIGHT AND/OR
OTHER APPLICABLE LAW. ANY USE OF THE WORK
OTHER THAN AS AUTHORIZED UNDER THIS LICENSE
OR COPYRIGHT LAW IS PROHIBITED.

BY EXERCISING ANY RIGHTS TO THE WORK PROVIDED
HERE, YOU ACCEPT AND AGREE TO BE BOUND BY THE
TERMS OF THIS LICENSE. TO THE EXTENT THIS
LICENSE MAY BE CONSIDERED TO BE A CONTRACT,
THE LICENSOR GRANTS YOU THE RIGHTS CONTAINED
HERE IN CONSIDERATION OF YOUR ACCEPTANCE OF
SUCH TERMS AND CONDITIONS.

Full license at http://creativecommons.org/licenses/by-nc/3.0/legalcode

/////////////////////////////////////////////////////////////////////////////**/

#include <api/g2/zcpar_symbol.h>
#include <Util.h>


zCPar_Symbol* zCPar_Symbol::create()
{
	typedef DWORD(__thiscall* ZCPAR_SYMBOL_CONSTRUCTOR)(void* thizz);
	static ZCPAR_SYMBOL_CONSTRUCTOR zCPar_SymbolConstructor = (ZCPAR_SYMBOL_CONSTRUCTOR)0x007A1690;

	// The symbol will be managed by the Gothic 2 exe and thus the appropriate new operator has to be called!
	void* memory = util::gothic2OperatorNew(0x3C); //sizeof(zCPar_Symbol) = 0x3C (-> see gothic 2 exe, address 0x0078DD02)
	ZeroMemory(memory, 0x3C);
	zCPar_SymbolConstructor(memory);
	return (zCPar_Symbol*)memory;
}