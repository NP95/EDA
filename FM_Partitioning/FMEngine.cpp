void FMEngine::createInitialPartition() {
    std::cout << "\nCreating initial partition..." << std::endl;
    
    // Get total number of cells
    int totalCells = netlist_->getCells().size();
    std::cout << "Total cells: " << totalCells << std::endl;
    
    // Calculate target sizes for each partition
    int targetSize0 = static_cast<int>(totalCells / (1.0 + balanceFactor_));
    int targetSize1 = totalCells - targetSize0;
    std::cout << "Target partition sizes: [" << targetSize0 << ", " << targetSize1 << "]" << std::endl;

    // Assign cells to partitions
    auto& cells = netlist_->getCells();
    for (int i = 0; i < totalCells; i++) {
        Cell* cell = &cells[i];
        std::cout << "Assigning cell " << cell->name << " to partition ";
        if (i < targetSize0) {
            cell->partition = 0;
            std::cout << "0" << std::endl;
        } else {
            cell->partition = 1;
            std::cout << "1" << std::endl;
        }
        
        // Update partition counts for all nets connected to this cell
        std::cout << "  Updating partition counts for connected nets:" << std::endl;
        for (Net* net : cell->nets) {
            std::cout << "    Net " << net->name << " before: [" 
                      << net->partitionCount[0] << ", " << net->partitionCount[1] << "]" << std::endl;
            net->partitionCount[cell->partition]++;
            std::cout << "    Net " << net->name << " after: [" 
                      << net->partitionCount[0] << ", " << net->partitionCount[1] << "]" << std::endl;
        }
    }

    // Update partition sizes
    partitionSize_[0] = targetSize0;
    partitionSize_[1] = targetSize1;
    std::cout << "Initial partition sizes set to: [" << partitionSize_[0] << ", " 
              << partitionSize_[1] << "]" << std::endl;
} 