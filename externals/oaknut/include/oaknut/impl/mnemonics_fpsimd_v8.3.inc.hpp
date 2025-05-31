// SPDX-FileCopyrightText: Copyright (c) 2023 merryhime <https://mary.rs>
// SPDX-License-Identifier: MIT

void FCADD(VReg_4H rd, VReg_4H rn, VReg_4H rm, Rot rot)
{
    if (rot != Rot::DEG_90 && rot != Rot::DEG_270)
        throw OaknutException{ExceptionType::InvalidRotation};
    emit<"00101110010mmmmm111r01nnnnnddddd", "r", "d", "n", "m">(static_cast<std::uint32_t>(rot) >> 1, rd, rn, rm);
}
void FCADD(VReg_8H rd, VReg_8H rn, VReg_8H rm, Rot rot)
{
    if (rot != Rot::DEG_90 && rot != Rot::DEG_270)
        throw OaknutException{ExceptionType::InvalidRotation};
    emit<"01101110010mmmmm111r01nnnnnddddd", "r", "d", "n", "m">(static_cast<std::uint32_t>(rot) >> 1, rd, rn, rm);
}
void FCADD(VReg_2S rd, VReg_2S rn, VReg_2S rm, Rot rot)
{
    if (rot != Rot::DEG_90 && rot != Rot::DEG_270)
        throw OaknutException{ExceptionType::InvalidRotation};
    emit<"00101110100mmmmm111r01nnnnnddddd", "r", "d", "n", "m">(static_cast<std::uint32_t>(rot) >> 1, rd, rn, rm);
}
void FCADD(VReg_4S rd, VReg_4S rn, VReg_4S rm, Rot rot)
{
    if (rot != Rot::DEG_90 && rot != Rot::DEG_270)
        throw OaknutException{ExceptionType::InvalidRotation};
    emit<"01101110100mmmmm111r01nnnnnddddd", "r", "d", "n", "m">(static_cast<std::uint32_t>(rot) >> 1, rd, rn, rm);
}
void FCADD(VReg_2D rd, VReg_2D rn, VReg_2D rm, Rot rot)
{
    if (rot != Rot::DEG_90 && rot != Rot::DEG_270)
        throw OaknutException{ExceptionType::InvalidRotation};
    emit<"01101110110mmmmm111r01nnnnnddddd", "r", "d", "n", "m">(static_cast<std::uint32_t>(rot) >> 1, rd, rn, rm);
}
void FCMLA(VReg_4H rd, VReg_4H rn, HElem em, Rot rot)
{
    if (em.reg_index() >= 16 || em.elem_index() >= 2)
        throw OaknutException{ExceptionType::InvalidCombination};
    emit<"0010111101LMmmmm0rr1H0nnnnnddddd", "r", "d", "n", "Mm", "H", "L">(rot, rd, rn, em.reg_index(), (em.elem_index() >> 1) & 1, em.elem_index() & 1);
}
void FCMLA(VReg_8H rd, VReg_8H rn, HElem em, Rot rot)
{
    if (em.reg_index() >= 16 || em.elem_index() >= 4)
        throw OaknutException{ExceptionType::InvalidCombination};
    emit<"0110111101LMmmmm0rr1H0nnnnnddddd", "r", "d", "n", "Mm", "H", "L">(rot, rd, rn, em.reg_index(), (em.elem_index() >> 1) & 1, em.elem_index() & 1);
}
void FCMLA(VReg_4S rd, VReg_4S rn, SElem em, Rot rot)
{
    if (em.reg_index() >= 16 || em.elem_index() >= 2)
        throw OaknutException{ExceptionType::InvalidCombination};
    emit<"0110111110LMmmmm0rr1H0nnnnnddddd", "r", "d", "n", "Mm", "H", "L">(rot, rd, rn, em.reg_index(), em.elem_index() & 1, 0);
}
void FCMLA(VReg_4H rd, VReg_4H rn, VReg_4H rm, Rot rot)
{
    emit<"00101110010mmmmm110rr1nnnnnddddd", "r", "d", "n", "m">(rot, rd, rn, rm);
}
void FCMLA(VReg_8H rd, VReg_8H rn, VReg_8H rm, Rot rot)
{
    emit<"01101110010mmmmm110rr1nnnnnddddd", "r", "d", "n", "m">(rot, rd, rn, rm);
}
void FCMLA(VReg_2S rd, VReg_2S rn, VReg_2S rm, Rot rot)
{
    emit<"00101110100mmmmm110rr1nnnnnddddd", "r", "d", "n", "m">(rot, rd, rn, rm);
}
void FCMLA(VReg_4S rd, VReg_4S rn, VReg_4S rm, Rot rot)
{
    emit<"01101110100mmmmm110rr1nnnnnddddd", "r", "d", "n", "m">(rot, rd, rn, rm);
}
void FCMLA(VReg_2D rd, VReg_2D rn, VReg_2D rm, Rot rot)
{
    emit<"01101110110mmmmm110rr1nnnnnddddd", "r", "d", "n", "m">(rot, rd, rn, rm);
}
void FJCVTZS(WReg wd, DReg rn)
{
    emit<"0001111001111110000000nnnnnddddd", "d", "n">(wd, rn);
}