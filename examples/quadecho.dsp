declare name 		"echo";
declare version 	"1.0";
declare author 		"Grame";
declare license 	"BSD";
declare copyright 	"(c)GRAME 2007";

//-----------------------------------------------
// 				A 1 second quadriphonic Echo
//-----------------------------------------------

import("music.lib");


process = vgroup("stereo echo", multi(echo1s, 4))
	with{ 
		multi(f,1) = f;
		multi(f,n) = f,multi(f,n-1);
	};							
	