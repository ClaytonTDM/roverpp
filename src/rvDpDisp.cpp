// Created by Unium on 04.03.26

#include "rvDpDisp.hpp"

namespace RV {
std::mutex g_objInstDispLock;
std::unordered_map<void *, CInstDisp> g_mapInstDisp;

std::mutex g_objDevDispLock;
std::unordered_map<void *, CDevDisp> g_mapDevDisp;
} // namespace RV
