/************************************************************************
 ************************************************************************
 FAUST compiler
 Copyright (C) 2003-2013 GRAME, Centre National de Creation Musicale
 ---------------------------------------------------------------------
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ************************************************************************
 ************************************************************************/

#ifndef _utilities_H
#define _utilities_H

#include <string>

using namespace std;

std::string searchIP();

string pathToContent(const string& path);

bool isInt(const char* word);

long lopt(char *argv[], const char *name, long def);

bool isopt(char *argv[], const char *name);

const char* loptions(char *argv[], const char *name, const char* def);

int lopt_Spe(int i, char *argv[], const char *name, char* path);

#endif
