#pragma once

#include <optional>
#include <string>
#include <vector>

class CoolerServiceProgram
{
public:
    int     Run(int argumentCount, char** argumentValues);

private:
    std::vector<std::string>    arguments;

    std::optional<std::string>  OptionValue(const std::string& name) const;
    static int                  PrintUsage();
};
