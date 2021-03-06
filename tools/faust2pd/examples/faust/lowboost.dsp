declare name 		"lowboost";
declare version 	"1.0";
declare author 		"Grame";
declare license 	"BSD";
declare copyright 	"(c)GRAME 2006";

//------------------------------------------------------------------
//	DAFX, Digital Audio Effects (Wiley ed.)
//	chapter 2 	: filters
//	section 2.3 : Equalizers
//	page 53 	: second order shelving filter design
//------------------------------------------------------------------

import("music.lib");


//------------------- low-frequency shelving boost (table 2.3) --------------------

V0(g)			= pow(10,g/20.0);
K(fc) 			= tan(PI*fc/SR);
square(x)		= x*x;
denom(fc)		= 1 + sqrt(2)*K(fc) + square(K(fc));

lfboost(fc, g)	= TF2(  (1 + sqrt(2*V0(g))*K(fc) + V0(g)*square(K(fc))) / denom(fc),
						 2 * (V0(g)*square(K(fc)) - 1) / denom(fc),
						(1 - sqrt(2*V0(g))*K(fc) + V0(g)*square(K(fc))) / denom(fc),
						 2 * (square(K(fc)) - 1) / denom(fc),
						(1 - sqrt(2)*K(fc) + square(K(fc))) / denom(fc)
					 );


//------------------------------ User Interface -----------------------------------

freq 				= hslider("freq", 1000, 20, 20000, 0.1);
gain				= hslider("gain (db)", 0, -20, 20, 0.1);


//----------------------------------- Process -------------------------------------

process 			= vgroup("low-freq shelving boost", lfboost(freq,gain));

