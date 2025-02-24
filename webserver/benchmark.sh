#!/bin/bash

echo "Starting tests..."

# Function to run tests
run_tests() {
    local server_name=$1
    local output_file="${server_name}_results.txt"
    
    echo "Testing ${server_name} Server..."
    echo "=== ${server_name} Server Results ===" > $output_file
    echo "" >> $output_file
    
    echo "Testing GET endpoint..."
    echo "GET / Results:" >> $output_file
    wrk -t4 -c100 -d10s http://localhost:8080/ 2>&1 | tee -a $output_file
    
    sleep 2
    
    echo "Testing POST endpoint..."
    echo "" >> $output_file
    echo "POST /data Results:" >> $output_file
    wrk -t4 -c100 -d10s -s post.lua http://localhost:8080/data 2>&1 | tee -a $output_file
    
    echo "Done testing ${server_name} server"
    echo "----------------------------------------"
}

# Test Rust server
echo "Please start the Rust server now and press Enter when ready..."
read

run_tests "Rust"

echo "Please stop the Rust server, start the C++ server, and press Enter when ready..."
read

run_tests "Cpp"

echo "All tests completed. Results are in Rust_results.txt and Cpp_results.txt"

# Print a summary
echo ""
echo "Summary:"
echo "========"
echo "Rust Server Results:"
grep "Requests/sec" Rust_results.txt
echo ""
echo "C++ Server Results:"
grep "Requests/sec" Cpp_results.txt
