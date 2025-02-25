//In circuit.hpp
class Circuit
{
    public:
    struct Node{
        std::string name;
        std::string type; //Gate type or "INPUT"
        int numInputs;
        std::vector<size_t> fanins;
        std::vector<size_t> fanouts;
        double arrivalTime;
        double requiredTime;
        double slack;
        double inputSlew;
        double outputSlew;
        double loadCapacitance;
    };

private:
      std::vector<Node> nodes_;
      std::unordered_map<std::string, size_t> nameToId_;
      std::vector<size_t> primaryInputs_;
      std::vector<size_t> primaryOututs_;
      std::vector<size_t> topoOrder_;

public:
      //Methods to add nodes and connections
      size_t addNode(const std::string& name, const stdLLstring& type, int numInputs = 0);
      void addConnection(const std::string& from,const std::string& to);

      //Accesor methods
      const Node& getNode(size_t id) const { return nodes_[id]}
      size_t getNodeId(const std::string& from, const std::string& to);

      //Friend class declaration to allow parser direct access
      friend class NetlistParser;
};


class Library{
   public:
         struct DelayTable {
            std::vector<double> inputSlews;
            std::vector<double> loadCaps;
            std::vector<std::vector<double>> delayValues;
            std::vecotr<std::vecotr<double>> slewValues;
            double interpolateDelay(double slew, double load) const;
            double interpolateSlew(double slew, double load) const;

         };

    private:
          std::unordered_map<std::string, DelayTable> gateTables_;
          double inverterCapacitance_;

    public:
         double getDelay(const std::string& gateType, double inputSlew, double loadCap, int numInputs = 2) const;
         double getOutputSlew(const std::string& gateType, double inputSlew, double loadCap, int numInputs = 2) const;
         double getInverterCapacitance() const {return inverterCapacitance_;}

        friend class LibertyParser;
}

bool Parser::openFile(){
    file_.open(filename_);
    if(!file_.is_open()) {
        std::cerr << "Error: Could not open file" << filename_ << std::endl;
        return false;
    }
    return true;
}

std::string Parser::getLine()
{
    std::string line;
    while(std::getline(file_,line))
    {
     size_t commentPos = line.find("//");
     if (commentPos != std::string::npos)
     {
        line = line.substr(0,commentPos);
     }
     
    line.erase(0, line.find_first_not_of("\t"));
    line.erase(line.find_last_not_of("\t")+1);

    if(!line.empty())
    {
        return line;
    }
    }
    return " ";
}

std::vector<std::string> Parser::tokenize(const std::string& line,char delimiter)
{
    std::vector<std::string> tokens;
    std::strinstream ss(line);
    std::string token;

    while (std::getline(ss, token, delimiter))
    {
        token.erase(0, token.find_first_not_of("\t"));
        token.erase(token.find_last_not_of("\t")+1);
        if(!token.empty())
        {
            tokens.push_back(token);
        }
    }
    return tokens;
}

//NetlistParser implementation
bool NetlistParser::parse()
{
    if(!openFile())
    return false;

std::string line;
while (!(line = getLine()).empty())
{
    if (line.find("INPUT") != std::string::npos) {
        if (!parseInputs(line)) return false;
    }
    else if (line.find("OUTPUT") != std::string::npos) {
        if (!parseOutputs(line)) return false;
    }
    else if (line.find("DFF") != std::string::npos) {
        if (!parseDFF(line)) return false;
    }
    else if (line.find("=") != std::string::npos) {
        if (!parseGate(line)) return false;
    }

}
return true;


}