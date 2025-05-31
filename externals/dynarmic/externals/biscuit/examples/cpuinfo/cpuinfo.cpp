// Copyright (c), 2022, KNS Group LLC (YADRO)
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include <biscuit/assembler.hpp>
#include <biscuit/cpuinfo.hpp>

#include <iostream>

using namespace biscuit;

int main()
{
    CPUInfo cpu;

    std::cout << "Has I:" <<  cpu.Has(RISCVExtension::I) << std::endl;
    std::cout << "Has M:" <<  cpu.Has(RISCVExtension::M) << std::endl;
    std::cout << "Has A:" <<  cpu.Has(RISCVExtension::A) << std::endl;
    std::cout << "Has F:" <<  cpu.Has(RISCVExtension::F) << std::endl;
    std::cout << "Has D:" <<  cpu.Has(RISCVExtension::D) << std::endl;
    std::cout << "Has C:" <<  cpu.Has(RISCVExtension::C) << std::endl;
    std::cout << "Has V:" <<  cpu.Has(RISCVExtension::V) << std::endl;

    if (cpu.Has(RISCVExtension::V)) {
        std::cout << "VLENB:" <<  cpu.GetVlenb() << std::endl;
    }

    return 0;
}
