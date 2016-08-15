/*
 *  Portable Runtime System (PRS)
 *  Copyright (C) 2016  Alexandre Tremblay
 *  
 *  This file is part of PRS.
 *  
 *  PRS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *  
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 *  portableruntimesystem@gmail.com
 */

/**
 * \file
 * \brief
 *  This file contains the main PRS application definitions.
 *
 *  This is the PRS application (main) entry point. If PRS is to be used as a library within another application, this
 *  file shall be excluded from the application and \ref prs_init shall be called from somewhere else.
 */

#include <prs/init.h>

int main(int argc, char* argv[])
{
    struct prs_init_params init_params = {
        .use_current_thread = PRS_TRUE,
        .core_mask = 0xFFFFFFFF
    };
    prs_init(&init_params);

    return 0;
}
