/*  Pcsx2 - Pc Ps2 Emulator
*  Copyright (C) 2009  Pcsx2 Team
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*  
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#pragma once

//------------------------------------------------------------------
// mVUupdateFlags() - Updates status/mac flags
//------------------------------------------------------------------

#define AND_XYZW			((_XYZW_SS && modXYZW) ? (1) : (mFLAG.doFlag ? (_X_Y_Z_W) : (flipMask[_X_Y_Z_W])))
#define ADD_XYZW			((_XYZW_SS && modXYZW) ? (_X ? 3 : (_Y ? 2 : (_Z ? 1 : 0))) : 0)
#define SHIFT_XYZW(gprReg)	{ if (_XYZW_SS && modXYZW && !_W) { SHL32ItoR(gprReg, ADD_XYZW); } }

// Note: If modXYZW is true, then it adjusts XYZW for Single Scalar operations
microVUt(void) mVUupdateFlags(mV, int reg, int regT1, bool modXYZW = 1) {
	int sReg, mReg = gprT1, xyzw = _X_Y_Z_W;
	static const u16 flipMask[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

	//SysPrintf("Status = %d; Mac = %d\n", sFLAG.doFlag, mFLAG.doFlag);
	if (mVUsFlagHack) { sFLAG.doFlag = 0; }
	if (!sFLAG.doFlag && !mFLAG.doFlag) { return; }
	if (!(!mFLAG.doFlag || (_XYZW_SS && modXYZW))) { 
		SSE2_PSHUFD_XMM_to_XMM(reg, reg, 0x1B); // Flip wzyx to xyzw
	}
	if (sFLAG.doFlag) {
		getFlagReg(sReg, sFLAG.write); // Set sReg to valid GPR by Cur Flag Instance
		mVUallocSFLAGa(sReg, sFLAG.lastWrite); // Get Prev Status Flag
		if (sFLAG.doNonSticky) AND32ItoR(sReg, 0xfffc00ff); // Clear O,U,S,Z flags
	}
	if (regT1 < 0) { regT1 = mVU->regAlloc->allocReg(); }

	//-------------------------Check for Signed flags------------------------------

	// The following code makes sure the Signed Bit isn't set with Negative Zero
	SSE_XORPS_XMM_to_XMM   (regT1, regT1); // Clear regT2
	SSE_CMPEQPS_XMM_to_XMM (regT1, reg);   // Set all F's if each vector is zero
	SSE_MOVMSKPS_XMM_to_R32(gprT2, regT1); // Used for Zero Flag Calculation
	SSE_ANDNPS_XMM_to_XMM  (regT1, reg);   // Used for Sign Flag Calculation
	SSE_MOVMSKPS_XMM_to_R32(mReg, regT1);  // Move the Sign Bits of the t1reg

	AND32ItoR(mReg, AND_XYZW);	// Grab "Is Signed" bits from the previous calculation
	SHL32ItoR(mReg, 4 + ADD_XYZW);

	//-------------------------Check for Zero flags------------------------------

	AND32ItoR(gprT2, AND_XYZW);	// Grab "Is Zero" bits from the previous calculation
	if (mFLAG.doFlag) { SHIFT_XYZW(gprT2); }
	OR32RtoR(mReg, gprT2);

	//-------------------------Write back flags------------------------------

	if (!(!mFLAG.doFlag || (_XYZW_SS && modXYZW))) { 
		SSE2_PSHUFD_XMM_to_XMM(reg, reg, 0x1B); // Flip wzyx to xyzw
	}
	if (mFLAG.doFlag) mVUallocMFLAGb(mVU, mReg, mFLAG.write); // Set Mac Flag
	if (sFLAG.doFlag) {
		OR32RtoR (sReg, mReg);
		if (sFLAG.doNonSticky) {
			SHL32ItoR(mReg, 8);
			OR32RtoR (sReg, mReg);
		}
	}
}

//------------------------------------------------------------------
// Helper Macros and Functions
//------------------------------------------------------------------

static void (*SSE_PS[]) (x86SSERegType, x86SSERegType) = { 
	SSE_ADDPS_XMM_to_XMM, // 0
	SSE_SUBPS_XMM_to_XMM, // 1
	SSE_MULPS_XMM_to_XMM, // 2
	SSE_MAXPS_XMM_to_XMM, // 3
	SSE_MINPS_XMM_to_XMM  // 4
};

static void (*SSE_SS[]) (x86SSERegType, x86SSERegType) = { 
	SSE_ADDSS_XMM_to_XMM, // 0 
	SSE_SUBSS_XMM_to_XMM, // 1
	SSE_MULSS_XMM_to_XMM, // 2
	SSE_MAXSS_XMM_to_XMM, // 3
	SSE_MINSS_XMM_to_XMM  // 4
};

// Prints Opcode to MicroProgram Logs
void mVU_printOP(microVU* mVU, int opCase, char* opName, bool isACC) {
	mVUlog(opName);
	opCase1 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogFt(); }
	opCase2 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogBC(); }
	opCase3 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogI();  }
	opCase4 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogQ();  }
}

// Sets Up Pass1 Info for Normal, BC, I, and Q Cases
void setupPass1(microVU* mVU, int opCase, bool isACC, bool noFlagUpdate) {
	opCase1 { mVUanalyzeFMAC1(mVU, ((isACC) ? 0 : _Fd_), _Fs_, _Ft_); }
	opCase2 { mVUanalyzeFMAC3(mVU, ((isACC) ? 0 : _Fd_), _Fs_, _Ft_); }
	opCase3 { mVUanalyzeFMAC1(mVU, ((isACC) ? 0 : _Fd_), _Fs_, 0); }
	opCase4 { mVUanalyzeFMAC1(mVU, ((isACC) ? 0 : _Fd_), _Fs_, 0); }
	if (noFlagUpdate) { sFLAG.doFlag = 0; }
}

// Sets Up Ft Reg for Normal, BC, I, and Q Cases
void setupFtReg(microVU* mVU, int& Ft, int opCase) {
	opCase1 { Ft = mVU->regAlloc->allocReg(_Ft_); }
	opCase2 { 
		if (!_XYZW_SS) {
			Ft = mVU->regAlloc->allocReg(_Ft_, 0, 0xf);
			mVUunpack_xyzw(Ft, Ft, _bc_);
		}
		else Ft = mVU->regAlloc->allocReg(_Ft_);
	}
	opCase3 { Ft = mVU->regAlloc->allocReg(); getIreg(Ft, 1); }
	opCase4 { Ft = mVU->regAlloc->allocReg(); getQreg(Ft); }
}

// Normal FMAC Opcodes
void mVU_FMACa(microVU* mVU, int recPass, int opCase, int opType, bool isACC, char* opName) {
	pass1 { setupPass1(mVU, opCase, isACC, ((opType == 3) || (opType == 4))); }
	pass2 {
		int Fs, Ft, ACC;
		mVU->regAlloc->reset(); // Reset for Testing
		setupFtReg(mVU, Ft, opCase);

		if (isACC) {
			ACC = mVU->regAlloc->allocReg((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);
			Fs  = mVU->regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
			if (_XYZW_SS && _X_Y_Z_W != 8) SSE2_PSHUFD_XMM_to_XMM(ACC, ACC, shuffleSS(_X_Y_Z_W));
		}
		else { Fs = mVU->regAlloc->allocReg(_Fs_, _Fd_, _X_Y_Z_W); }

		opCase1 { if (_XYZW_SS && _X_Y_Z_W != 8) { SSE2_PSHUFD_XMM_to_XMM(Ft, Ft, shuffleSS(_X_Y_Z_W)); } }
		opCase2 { if (_XYZW_SS && (!_bc_x))		 { SSE2_PSHUFD_XMM_to_XMM(Ft, Ft, shuffleSS((1 << (3 - _bc_)))); } }

		if (_XYZW_SS) SSE_SS[opType](Fs, Ft);
		else		  SSE_PS[opType](Fs, Ft);

		opCase1 { if (_XYZW_SS && _X_Y_Z_W != 8) { SSE2_PSHUFD_XMM_to_XMM(Ft, Ft, shuffleSS(_X_Y_Z_W)); } }
		opCase2 { if (_XYZW_SS && (!_bc_x))		 { SSE2_PSHUFD_XMM_to_XMM(Ft, Ft, shuffleSS((1 << (3 - _bc_)))); } }

		if (isACC) {
			if (_XYZW_SS) SSE_MOVSS_XMM_to_XMM(ACC, Fs);
			else		  mVUmergeRegs(ACC, Fs, _X_Y_Z_W);
			mVUupdateFlags(mVU, ACC, Fs);
			if (_XYZW_SS && _X_Y_Z_W != 8) SSE2_PSHUFD_XMM_to_XMM(ACC, ACC, shuffleSS(_X_Y_Z_W));
			mVU->regAlloc->clearNeeded(ACC);
		}
		else mVUupdateFlags(mVU, Fs, (((opCase==2)&&(!_XYZW_SS)) ? Ft : -1));

		//if (isACC) SSE_MOVAPS_XMM_to_XMM(xmmACC, ACC); // For Testing
		mVU->regAlloc->clearNeeded(Fs); // Always Clear Written Reg First
		mVU->regAlloc->clearNeeded(Ft);
		mVU->regAlloc->flushAll(); // Flush All for Testing
	}
	pass3 { mVU_printOP(mVU, opCase, opName, isACC); }
}

// MADDA/MSUBA Opcodes
void mVU_FMACb(microVU* mVU, int recPass, int opCase, int opType, char* opName) {
	pass1 { setupPass1(mVU, opCase, 1, 0); }
	pass2 {
		int Fs, Ft, ACC;
		mVU->regAlloc->reset(); // Reset for Testing
		setupFtReg(mVU, Ft, opCase);

		ACC = mVU->regAlloc->allocReg(32, 32, 0xf, 0);
		Fs  = mVU->regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);

		if (_XYZW_SS && _X_Y_Z_W != 8)			 { SSE2_PSHUFD_XMM_to_XMM(ACC, ACC, shuffleSS(_X_Y_Z_W)); }
		opCase1 { if (_XYZW_SS && _X_Y_Z_W != 8) { SSE2_PSHUFD_XMM_to_XMM(Ft,  Ft,  shuffleSS(_X_Y_Z_W)); } }
		opCase2 { if (_XYZW_SS && (!_bc_x))		 { SSE2_PSHUFD_XMM_to_XMM(Ft,  Ft,  shuffleSS((1 << (3 - _bc_)))); } }

		if (_XYZW_SS) SSE_SS[2](Fs, Ft);
		else		  SSE_PS[2](Fs, Ft);

		opCase1 { if (_XYZW_SS && _X_Y_Z_W != 8) { SSE2_PSHUFD_XMM_to_XMM(Ft,  Ft,  shuffleSS(_X_Y_Z_W)); } }
		opCase2 { if (_XYZW_SS && (!_bc_x))		 { SSE2_PSHUFD_XMM_to_XMM(Ft,  Ft,  shuffleSS((1 << (3 - _bc_)))); } }

		if (_XYZW_SS || _X_Y_Z_W == 0xf) {
			if (_XYZW_SS) SSE_SS[opType](ACC, Fs);
			else		  SSE_PS[opType](ACC, Fs);	  
			mVUupdateFlags(mVU, ACC, Fs);
			if (_XYZW_SS && _X_Y_Z_W != 8) SSE2_PSHUFD_XMM_to_XMM(ACC, ACC, shuffleSS(_X_Y_Z_W));
		}
		else {
			int tempACC = mVU->regAlloc->allocReg();
			SSE_MOVAPS_XMM_to_XMM(tempACC, ACC);
			SSE_PS[opType](tempACC, Fs);
			mVUmergeRegs(ACC, tempACC, _X_Y_Z_W);
			mVU->regAlloc->clearNeeded(tempACC);
		}

		//SSE_MOVAPS_XMM_to_XMM(xmmACC, ACC); // For Testing
		mVU->regAlloc->clearNeeded(ACC);
		mVU->regAlloc->clearNeeded(Fs);
		mVU->regAlloc->clearNeeded(Ft);
		mVU->regAlloc->flushAll(); // Flush All for Testing
	}
	pass3 { mVU_printOP(mVU, opCase, opName, 1); }
}

// MADD Opcodes
void mVU_FMACc(microVU* mVU, int recPass, int opCase, char* opName) {
	pass1 { setupPass1(mVU, opCase, 0, 0); }
	pass2 {
		int Fs, Ft, ACC;
		mVU->regAlloc->reset(); // Reset for Testing
		setupFtReg(mVU, Ft, opCase);

		ACC = mVU->regAlloc->allocReg(32);
		Fs  = mVU->regAlloc->allocReg(_Fs_, _Fd_, _X_Y_Z_W);

		if (_XYZW_SS && _X_Y_Z_W != 8)			 { SSE2_PSHUFD_XMM_to_XMM(ACC, ACC, shuffleSS(_X_Y_Z_W)); }
		opCase1 { if (_XYZW_SS && _X_Y_Z_W != 8) { SSE2_PSHUFD_XMM_to_XMM(Ft,  Ft,  shuffleSS(_X_Y_Z_W)); } }
		opCase2 { if (_XYZW_SS && (!_bc_x))		 { SSE2_PSHUFD_XMM_to_XMM(Ft,  Ft,  shuffleSS((1 << (3 - _bc_)))); } }

		if (_XYZW_SS) { SSE_SS[2](Fs, Ft); SSE_SS[0](Fs, ACC); }
		else		  { SSE_PS[2](Fs, Ft); SSE_PS[0](Fs, ACC); }

		mVUupdateFlags(mVU, Fs, -1);

		if (_XYZW_SS && _X_Y_Z_W != 8)			 { SSE2_PSHUFD_XMM_to_XMM(ACC, ACC, shuffleSS(_X_Y_Z_W)); }
		opCase1 { if (_XYZW_SS && _X_Y_Z_W != 8) { SSE2_PSHUFD_XMM_to_XMM(Ft,  Ft,  shuffleSS(_X_Y_Z_W)); } }
		opCase2 { if (_XYZW_SS && (!_bc_x))		 { SSE2_PSHUFD_XMM_to_XMM(Ft,  Ft,  shuffleSS((1 << (3 - _bc_)))); } }

		mVU->regAlloc->clearNeeded(ACC);
		mVU->regAlloc->clearNeeded(Fs); // Always Clear Written Reg First
		mVU->regAlloc->clearNeeded(Ft);
		mVU->regAlloc->flushAll(); // Flush All for Testing
	}
	pass3 { mVU_printOP(mVU, opCase, opName, 0); }
}

// MSUB Opcodes
void mVU_FMACd(microVU* mVU, int recPass, int opCase, char* opName) {
	pass1 { setupPass1(mVU, opCase, 0, 0); }
	pass2 {
		int Fs, Ft, Fd;
		mVU->regAlloc->reset(); // Reset for Testing
		setupFtReg(mVU, Ft, opCase);

		Fs  = mVU->regAlloc->allocReg(_Fs_, 0,  _X_Y_Z_W);
		Fd  = mVU->regAlloc->allocReg(32, _Fd_, _X_Y_Z_W);

		opCase1 { if (_XYZW_SS && _X_Y_Z_W != 8) { SSE2_PSHUFD_XMM_to_XMM(Ft, Ft, shuffleSS(_X_Y_Z_W)); } }
		opCase2 { if (_XYZW_SS && (!_bc_x))		 { SSE2_PSHUFD_XMM_to_XMM(Ft, Ft, shuffleSS((1 << (3 - _bc_)))); } }

		if (_XYZW_SS) { SSE_SS[2](Fs, Ft); SSE_SS[1](Fd, Fs); }
		else		  { SSE_PS[2](Fs, Ft); SSE_PS[1](Fd, Fs); }

		mVUupdateFlags(mVU, Fd, Fs);

		opCase1 { if (_XYZW_SS && _X_Y_Z_W != 8) { SSE2_PSHUFD_XMM_to_XMM(Ft, Ft, shuffleSS(_X_Y_Z_W)); } }
		opCase2 { if (_XYZW_SS && (!_bc_x))		 { SSE2_PSHUFD_XMM_to_XMM(Ft, Ft, shuffleSS((1 << (3 - _bc_)))); } }

		mVU->regAlloc->clearNeeded(Fd); // Always Clear Written Reg First
		mVU->regAlloc->clearNeeded(Ft);
		mVU->regAlloc->clearNeeded(Fs);
		mVU->regAlloc->flushAll(); // Flush All for Testing
	}
	pass3 { mVU_printOP(mVU, opCase, opName, 0); }
}

// ABS Opcode
mVUop(mVU_ABS) {
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2 { 
		if (!_Ft_) return;
		mVU->regAlloc->reset(); // Reset for Testing
		int Fs = mVU->regAlloc->allocReg(_Fs_, _Ft_, _X_Y_Z_W, ((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
		SSE_ANDPS_M128_to_XMM(Fs, (uptr)mVU_absclip);
		mVU->regAlloc->clearNeeded(Fs);
		mVU->regAlloc->flushAll(); // Flush All for Testing
	}
	pass3 { mVUlog("ABS"); mVUlogFtFs(); }
}

// OPMULA Opcode
mVUop(mVU_OPMULA) { 
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2 {
		mVU->regAlloc->reset(); // Reset for Testing
		int Fs = mVU->regAlloc->allocReg(_Fs_, 32, _X_Y_Z_W);
		int Ft = mVU->regAlloc->allocReg(_Ft_,  0, _X_Y_Z_W);

		SSE2_PSHUFD_XMM_to_XMM(Fs, Fs, 0xC9); // WXZY
		SSE2_PSHUFD_XMM_to_XMM(Ft, Ft, 0xD2); // WYXZ
		SSE_MULPS_XMM_to_XMM(Fs, Ft);
		mVUupdateFlags(mVU, Fs, Ft);

		mVU->regAlloc->clearNeeded(Fs);
		mVU->regAlloc->clearNeeded(Ft);
		mVU->regAlloc->flushAll(); // Flush All for Testing
	}
	pass3 { mVUlog("OPMULA"); mVUlogACC(); mVUlogFt(); }
}

// OPMSUB Opcode
mVUop(mVU_OPMSUB) { 
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 {
		mVU->regAlloc->reset(); // Reset for Testing
		int Ft  = mVU->regAlloc->allocReg(_Ft_);
		int Fs  = mVU->regAlloc->allocReg(_Fs_,  0, _X_Y_Z_W);
		int ACC = mVU->regAlloc->allocReg(32, _Fd_, _X_Y_Z_W);

		SSE2_PSHUFD_XMM_to_XMM(Fs, Fs, 0xC9); // WXZY
		SSE2_PSHUFD_XMM_to_XMM(Ft, Ft, 0xD2); // WYXZ
		SSE_MULPS_XMM_to_XMM(Fs,  Ft);
		SSE_SUBPS_XMM_to_XMM(ACC, Fs);
		mVUupdateFlags(mVU, ACC, Fs);

		mVU->regAlloc->clearNeeded(32);
		mVU->regAlloc->clearNeeded(Fs);
		mVU->regAlloc->clearNeeded(Ft);
		mVU->regAlloc->flushAll(); // Flush All for Testing
		
	}
	pass3 { mVUlog("OPMSUB"); mVUlogFd(); mVUlogFt(); }
}

// FTOI0/FTIO4/FTIO12/FTIO15 Opcodes
void mVU_FTOIx(mP, uptr addr, char* opName) {
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2 { 
		if (!_Ft_) return;
		mVU->regAlloc->reset(); // Reset for Testing
		int Fs = mVU->regAlloc->allocReg(_Fs_, _Ft_, _X_Y_Z_W, ((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
		int t1 = mVU->regAlloc->allocReg();
		int t2 = mVU->regAlloc->allocReg();

		// Note: For help understanding this algorithm see recVUMI_FTOI_Saturate()
		SSE_MOVAPS_XMM_to_XMM(t1, Fs);
		if (addr) { SSE_MULPS_M128_to_XMM(Fs, addr); }
		SSE2_CVTTPS2DQ_XMM_to_XMM(Fs, Fs);
		SSE2_PXOR_M128_to_XMM(t1, (uptr)mVU_signbit);
		SSE2_PSRAD_I8_to_XMM (t1, 31);
		SSE_MOVAPS_XMM_to_XMM(t2, Fs);
		SSE2_PCMPEQD_M128_to_XMM(t2, (uptr)mVU_signbit);
		SSE_ANDPS_XMM_to_XMM (t1, t2);
		SSE2_PADDD_XMM_to_XMM(Fs, t1);

		mVU->regAlloc->clearNeeded(Fs);
		mVU->regAlloc->clearNeeded(t1);
		mVU->regAlloc->clearNeeded(t2);
		mVU->regAlloc->flushAll(); // Flush All for Testing
	}
	pass3 { mVUlog(opName); mVUlogFtFs(); }
}

// ITOF0/ITOF4/ITOF12/ITOF15 Opcodes
void mVU_ITOFx(mP, uptr addr, char* opName) {
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2 { 
		if (!_Ft_) return;
		mVU->regAlloc->reset(); // Reset for Testing
		int Fs = mVU->regAlloc->allocReg(_Fs_, _Ft_, _X_Y_Z_W, ((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
		
		SSE2_CVTDQ2PS_XMM_to_XMM(Fs, Fs);
		if (addr) { SSE_MULPS_M128_to_XMM(Fs, addr); }
		//mVUclamp2(Fs, xmmT1, 15); // Clamp (not sure if this is needed)
		
		mVU->regAlloc->clearNeeded(Fs);
		mVU->regAlloc->flushAll(); // Flush All for Testing
	}
	pass3 { mVUlog(opName); mVUlogFtFs(); }
}

// Clip Opcode
mVUop(mVU_CLIP) {
	pass1 { mVUanalyzeFMAC4(mVU, _Fs_, _Ft_); }
	pass2 {
		mVU->regAlloc->reset(); // Reset for Testing
		int Fs = mVU->regAlloc->allocReg(_Fs_);
		int Ft = mVU->regAlloc->allocReg(_Ft_, 0, 1);
		int t1 = mVU->regAlloc->allocReg();

		mVUunpack_xyzw(Ft, Ft, 0);
		mVUallocCFLAGa(mVU, gprT1, cFLAG.lastWrite);
		SHL32ItoR(gprT1, 6);

		SSE_ANDPS_M128_to_XMM(Ft, (uptr)mVU_absclip);
		SSE_MOVAPS_XMM_to_XMM(t1, Ft);
		SSE_ORPS_M128_to_XMM(t1, (uptr)mVU_signbit);

		SSE_CMPNLEPS_XMM_to_XMM(t1, Fs); // -w, -z, -y, -x
		SSE_CMPLTPS_XMM_to_XMM(Ft, Fs);  // +w, +z, +y, +x

		SSE_MOVAPS_XMM_to_XMM(Fs, Ft);	 // Fs = +w, +z, +y, +x
		SSE_UNPCKLPS_XMM_to_XMM(Ft, t1); // Ft = -y,+y,-x,+x
		SSE_UNPCKHPS_XMM_to_XMM(Fs, t1); // Fs = -w,+w,-z,+z

		SSE_MOVMSKPS_XMM_to_R32(gprT2, Fs); // -w,+w,-z,+z
		AND32ItoR(gprT2, 0x3);
		SHL32ItoR(gprT2, 4);
		OR32RtoR (gprT1, gprT2);

		SSE_MOVMSKPS_XMM_to_R32(gprT2, Ft); // -y,+y,-x,+x
		AND32ItoR(gprT2, 0xf);
		OR32RtoR (gprT1, gprT2);
		AND32ItoR(gprT1, 0xffffff);

		mVUallocCFLAGb(mVU, gprT1, cFLAG.write);
		mVU->regAlloc->clearNeeded(Fs);
		mVU->regAlloc->clearNeeded(Ft);
		mVU->regAlloc->clearNeeded(t1);
		mVU->regAlloc->flushAll(); // Flush All for Testing
	}
	pass3 { mVUlog("CLIP"); mVUlogCLIP(); }
}

//------------------------------------------------------------------
// Micro VU Micromode Upper instructions
//------------------------------------------------------------------

mVUop(mVU_ADD)		{ mVU_FMACa(mVU, recPass, 1, 0, 0,	"ADD");    }
mVUop(mVU_ADDi)		{ mVU_FMACa(mVU, recPass, 3, 0, 0,	"ADDi");   }
mVUop(mVU_ADDq)		{ mVU_FMACa(mVU, recPass, 4, 0, 0,	"ADDq");   }
mVUop(mVU_ADDx)		{ mVU_FMACa(mVU, recPass, 2, 0, 0,	"ADDx");   }
mVUop(mVU_ADDy)		{ mVU_FMACa(mVU, recPass, 2, 0, 0,	"ADDy");   }
mVUop(mVU_ADDz)		{ mVU_FMACa(mVU, recPass, 2, 0, 0,	"ADDz");   }
mVUop(mVU_ADDw)		{ mVU_FMACa(mVU, recPass, 2, 0, 0,	"ADDw");   }
mVUop(mVU_ADDA)		{ mVU_FMACa(mVU, recPass, 1, 0, 1,	"ADDA");   }
mVUop(mVU_ADDAi)	{ mVU_FMACa(mVU, recPass, 3, 0, 1,	"ADDAi");  }
mVUop(mVU_ADDAq)	{ mVU_FMACa(mVU, recPass, 4, 0, 1,	"ADDAq");  }
mVUop(mVU_ADDAx)	{ mVU_FMACa(mVU, recPass, 2, 0, 1,	"ADDAx");  }
mVUop(mVU_ADDAy)	{ mVU_FMACa(mVU, recPass, 2, 0, 1,	"ADDAy");  }
mVUop(mVU_ADDAz)	{ mVU_FMACa(mVU, recPass, 2, 0, 1,	"ADDAz");  }
mVUop(mVU_ADDAw)	{ mVU_FMACa(mVU, recPass, 2, 0, 1,	"ADDAw");  }
mVUop(mVU_SUB)		{ mVU_FMACa(mVU, recPass, 1, 1, 0,	"SUB");    }
mVUop(mVU_SUBi)		{ mVU_FMACa(mVU, recPass, 3, 1, 0,	"SUBi");   }
mVUop(mVU_SUBq)		{ mVU_FMACa(mVU, recPass, 4, 1, 0,	"SUBq");   }
mVUop(mVU_SUBx)		{ mVU_FMACa(mVU, recPass, 2, 1, 0,	"SUBx");   }
mVUop(mVU_SUBy)		{ mVU_FMACa(mVU, recPass, 2, 1, 0,	"SUBy");   }
mVUop(mVU_SUBz)		{ mVU_FMACa(mVU, recPass, 2, 1, 0,	"SUBz");   }
mVUop(mVU_SUBw)		{ mVU_FMACa(mVU, recPass, 2, 1, 0,	"SUBw");   }
mVUop(mVU_SUBA)		{ mVU_FMACa(mVU, recPass, 1, 1, 1,	"SUBA");   }
mVUop(mVU_SUBAi)	{ mVU_FMACa(mVU, recPass, 3, 1, 1,	"SUBAi");  }
mVUop(mVU_SUBAq)	{ mVU_FMACa(mVU, recPass, 4, 1, 1,	"SUBAq");  }
mVUop(mVU_SUBAx)	{ mVU_FMACa(mVU, recPass, 2, 1, 1,	"SUBAx");  }
mVUop(mVU_SUBAy)	{ mVU_FMACa(mVU, recPass, 2, 1, 1,	"SUBAy");  }
mVUop(mVU_SUBAz)	{ mVU_FMACa(mVU, recPass, 2, 1, 1,	"SUBAz");  }
mVUop(mVU_SUBAw)	{ mVU_FMACa(mVU, recPass, 2, 1, 1,	"SUBAw");  }
mVUop(mVU_MUL)		{ mVU_FMACa(mVU, recPass, 1, 2, 0,	"MUL");    }
mVUop(mVU_MULi)		{ mVU_FMACa(mVU, recPass, 3, 2, 0,	"MULi");   }
mVUop(mVU_MULq)		{ mVU_FMACa(mVU, recPass, 4, 2, 0,	"MULq");   }
mVUop(mVU_MULx)		{ mVU_FMACa(mVU, recPass, 2, 2, 0,	"MULx");   }
mVUop(mVU_MULy)		{ mVU_FMACa(mVU, recPass, 2, 2, 0,	"MULy");   }
mVUop(mVU_MULz)		{ mVU_FMACa(mVU, recPass, 2, 2, 0,	"MULz");   }
mVUop(mVU_MULw)		{ mVU_FMACa(mVU, recPass, 2, 2, 0,	"MULw");   }
mVUop(mVU_MULA)		{ mVU_FMACa(mVU, recPass, 1, 2, 1,	"MULA");   }
mVUop(mVU_MULAi)	{ mVU_FMACa(mVU, recPass, 3, 2, 1,	"MULAi");  }
mVUop(mVU_MULAq)	{ mVU_FMACa(mVU, recPass, 4, 2, 1,	"MULAq");  }
mVUop(mVU_MULAx)	{ mVU_FMACa(mVU, recPass, 2, 2, 1,	"MULAx");  }
mVUop(mVU_MULAy)	{ mVU_FMACa(mVU, recPass, 2, 2, 1,	"MULAy");  }
mVUop(mVU_MULAz)	{ mVU_FMACa(mVU, recPass, 2, 2, 1,	"MULAz");  }
mVUop(mVU_MULAw)	{ mVU_FMACa(mVU, recPass, 2, 2, 1,	"MULAw");  }
mVUop(mVU_MADD)		{ mVU_FMACc(mVU, recPass, 1,		"MADD");   }
mVUop(mVU_MADDi)	{ mVU_FMACc(mVU, recPass, 3,		"MADDi");  }
mVUop(mVU_MADDq)	{ mVU_FMACc(mVU, recPass, 4,		"MADDq");  }
mVUop(mVU_MADDx)	{ mVU_FMACc(mVU, recPass, 2,		"MADDx");  }
mVUop(mVU_MADDy)	{ mVU_FMACc(mVU, recPass, 2,		"MADDy");  }
mVUop(mVU_MADDz)	{ mVU_FMACc(mVU, recPass, 2,		"MADDz");  }
mVUop(mVU_MADDw)	{ mVU_FMACc(mVU, recPass, 2,		"MADDw");  }
mVUop(mVU_MADDA)	{ mVU_FMACb(mVU, recPass, 1, 0,		"MADDA");  }
mVUop(mVU_MADDAi)	{ mVU_FMACb(mVU, recPass, 3, 0,		"MADDAi"); }
mVUop(mVU_MADDAq)	{ mVU_FMACb(mVU, recPass, 4, 0,		"MADDAq"); }
mVUop(mVU_MADDAx)	{ mVU_FMACb(mVU, recPass, 2, 0,		"MADDAx"); }
mVUop(mVU_MADDAy)	{ mVU_FMACb(mVU, recPass, 2, 0,		"MADDAy"); }
mVUop(mVU_MADDAz)	{ mVU_FMACb(mVU, recPass, 2, 0,		"MADDAz"); }
mVUop(mVU_MADDAw)	{ mVU_FMACb(mVU, recPass, 2, 0,		"MADDAw"); }
mVUop(mVU_MSUB)		{ mVU_FMACd(mVU, recPass, 1,		"MSUB");   }
mVUop(mVU_MSUBi)	{ mVU_FMACd(mVU, recPass, 3,		"MSUBi");  }
mVUop(mVU_MSUBq)	{ mVU_FMACd(mVU, recPass, 4,		"MSUBq");  }
mVUop(mVU_MSUBx)	{ mVU_FMACd(mVU, recPass, 2,		"MSUBx");  }
mVUop(mVU_MSUBy)	{ mVU_FMACd(mVU, recPass, 2,		"MSUBy");  }
mVUop(mVU_MSUBz)	{ mVU_FMACd(mVU, recPass, 2,		"MSUBz");  }
mVUop(mVU_MSUBw)	{ mVU_FMACd(mVU, recPass, 2,		"MSUBw");  }
mVUop(mVU_MSUBA)	{ mVU_FMACb(mVU, recPass, 1, 1,		"MSUBA");  }
mVUop(mVU_MSUBAi)	{ mVU_FMACb(mVU, recPass, 3, 1,		"MSUBAi"); }
mVUop(mVU_MSUBAq)	{ mVU_FMACb(mVU, recPass, 4, 1,		"MSUBAq"); }
mVUop(mVU_MSUBAx)	{ mVU_FMACb(mVU, recPass, 2, 1,		"MSUBAx"); }
mVUop(mVU_MSUBAy)	{ mVU_FMACb(mVU, recPass, 2, 1,		"MSUBAy"); }
mVUop(mVU_MSUBAz)	{ mVU_FMACb(mVU, recPass, 2, 1,		"MSUBAz"); }
mVUop(mVU_MSUBAw)	{ mVU_FMACb(mVU, recPass, 2, 1,		"MSUBAw"); }
mVUop(mVU_MAX)		{ mVU_FMACa(mVU, recPass, 1, 3, 0,	"MAX");    }
mVUop(mVU_MAXi)		{ mVU_FMACa(mVU, recPass, 3, 3, 0,	"MAXi");   }
mVUop(mVU_MAXx)		{ mVU_FMACa(mVU, recPass, 2, 3, 0,	"MAXx");   }
mVUop(mVU_MAXy)		{ mVU_FMACa(mVU, recPass, 2, 3, 0,	"MAXy");   }
mVUop(mVU_MAXz)		{ mVU_FMACa(mVU, recPass, 2, 3, 0,	"MAXz");   }
mVUop(mVU_MAXw)		{ mVU_FMACa(mVU, recPass, 2, 3, 0,	"MAXw");   }
mVUop(mVU_MINI)		{ mVU_FMACa(mVU, recPass, 1, 4, 0,	"MINI");   }
mVUop(mVU_MINIi)	{ mVU_FMACa(mVU, recPass, 3, 4, 0,	"MINIi");  }
mVUop(mVU_MINIx)	{ mVU_FMACa(mVU, recPass, 2, 4, 0,	"MINIx");  }
mVUop(mVU_MINIy)	{ mVU_FMACa(mVU, recPass, 2, 4, 0,	"MINIy");  }
mVUop(mVU_MINIz)	{ mVU_FMACa(mVU, recPass, 2, 4, 0,	"MINIz");  }
mVUop(mVU_MINIw)	{ mVU_FMACa(mVU, recPass, 2, 4, 0,	"MINIw");  }
mVUop(mVU_FTOI0)	{ mVU_FTOIx(mX, (uptr)0,			"FTOI0");  }
mVUop(mVU_FTOI4)	{ mVU_FTOIx(mX, (uptr)mVU_FTOI_4,	"FTOI4");  }
mVUop(mVU_FTOI12)	{ mVU_FTOIx(mX, (uptr)mVU_FTOI_12,	"FTOI12"); }
mVUop(mVU_FTOI15)	{ mVU_FTOIx(mX, (uptr)mVU_FTOI_15,	"FTOI15"); }
mVUop(mVU_ITOF0)	{ mVU_ITOFx(mX, (uptr)0,			"ITOF0");  }
mVUop(mVU_ITOF4)	{ mVU_ITOFx(mX, (uptr)mVU_ITOF_4,	"ITOF4");  }
mVUop(mVU_ITOF12)	{ mVU_ITOFx(mX, (uptr)mVU_ITOF_12,	"ITOF12"); }
mVUop(mVU_ITOF15)	{ mVU_ITOFx(mX, (uptr)mVU_ITOF_15,	"ITOF15"); }
mVUop(mVU_NOP)		{ pass3 { mVUlog("NOP"); } }
