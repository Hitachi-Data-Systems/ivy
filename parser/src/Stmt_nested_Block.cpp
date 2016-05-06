//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include "Stmt_nested_Block.h"

void Stmt_nested_Block::display(const std::string& indent, std::ostream& os)
{
    os << indent << "Stmt_nested_block at " << bookmark << " on:" << std::endl;

    if (p_Block != nullptr)
    {
        p_Block->display(indent+indent_increment,os);
    }
    else os << indent << "<Stmt_nested_block\'s pointer to nested Block == nullptr.>" << std::endl;
}
