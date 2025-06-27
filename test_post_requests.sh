#!/bin/bash

# Configuration
SERVER_HOST="localhost"
SERVER_PORT_8080="8080"
SERVER_PORT_8081="8081"
UPLOAD_DIR="/Users/pier-lucrichard/prog/webserv/www/uploads"
CLIENT_MAX_BODY_SIZE_MB=10

# --- Helper Functions ---

function run_test() {
    local test_name="$1"
    local command="$2"
    local expected_status="$3"
    local cleanup_files="$4"

    echo "\n--- Running Test: $test_name ---"
    echo "Command: $command"

    # Run the command and capture output and status code
    # Use -s to silent progress, and -i to include headers
    local modified_command=${command//-v/-s -i}
    HTTP_RESPONSE=$(eval "$modified_command")
    HTTP_STATUS=$(echo "$HTTP_RESPONSE" | grep "HTTP/1.1" | awk '{print $2}' | tail -n 1 | tr -d '\r')

    echo "HTTP Status: $HTTP_STATUS (Expected: $expected_status)"

    if [[ "$HTTP_STATUS" == "$expected_status" ]]; then
        echo "RESULT: PASS"
    else
        echo "RESULT: FAIL"
        echo "Response:"
        # With -i, the response includes headers, so let's print it all
        echo "$HTTP_RESPONSE"
    fi

    # Clean up files if specified
    if [[ -n "$cleanup_files" ]]; then
        rm -f $cleanup_files
    fi
}

function cleanup_all() {
    echo "\n--- Cleaning up all test files ---"
    rm -f test_upload.txt another_test.txt large_file.bin
    rm -f "$UPLOAD_DIR/test_upload.txt"
    rm -f "$UPLOAD_DIR/large_file.bin"
    rm -f "$UPLOAD_DIR/nonexistent_dir/another_test.txt" # In case it was created
    rm -f /Users/pier-lucrichard/prog/webserv/www/delete_me.txt
    rm -f /Users/pier-lucrichard/prog/webserv/www/cgi-bin/test_post.php
    echo "Cleanup complete."
}

# --- Main Test Execution ---

cleanup_all # Ensure a clean slate before starting

# --- Pre-test Setup ---
echo "\n--- Preparing environment for tests ---"
# Create a file to be deleted by a test
touch /Users/pier-lucrichard/prog/webserv/www/delete_me.txt

# Create a dummy CGI script for POST testing
cat << 'EOF' > /Users/pier-lucrichard/prog/webserv/www/cgi-bin/test_post.php
<?php
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    echo "<html><body><h1>POST Data Received</h1>";
    echo "<p>Name: " . htmlspecialchars($_POST['name']) . "</p>";
    echo "</body></html>";
} else {
    echo "<html><body><h1>No POST data received.</h1></body></html>";
}
?>
EOF
chmod +x /Users/pier-lucrichard/prog/webserv/www/cgi-bin/test_post.php
echo "Test environment ready."

# Test 1: Successful File Upload (Basic POST)
echo "Hello, this is a test file." > test_upload.txt
run_test \
    "Successful File Upload" \
    "curl -v -X POST --data-binary \"@test_upload.txt\" http://${SERVER_HOST}:${SERVER_PORT_8080}/uploads/test_upload.txt" \
    "201" \
    "test_upload.txt $UPLOAD_DIR/test_upload.txt"

# Test 2: File Upload to a Non-Existent Directory (Error Handling)
echo "Another test." > another_test.txt
run_test \
    "File Upload to Non-Existent Directory" \
    "curl -v -X POST --data-binary \"@another_test.txt\" http://${SERVER_HOST}:${SERVER_PORT_8080}/uploads/nonexistent_dir/another_test.txt" \
    "500" \
    "another_test.txt $UPLOAD_DIR/nonexistent_dir/another_test.txt"

# Test 3: Exceeding client_max_body_size
dd if=/dev/urandom of=large_file.bin bs=1M count=$((CLIENT_MAX_BODY_SIZE_MB + 5)) 2>/dev/null
run_test \
    "Exceeding Client Max Body Size" \
    "curl -v -X POST --data-binary \"@large_file.bin\" http://${SERVER_HOST}:${SERVER_PORT_8080}/uploads/large_file.bin" \
    "413" \
    "large_file.bin $UPLOAD_DIR/large_file.bin"

# Test 4: POST to a Location Without POST Method Allowed
echo "Forbidden post." > forbidden_post.txt
run_test \
    "POST to Forbidden Location" \
    "curl -v -X POST --data-binary \"@forbidden_post.txt\" http://${SERVER_HOST}:${SERVER_PORT_8081}/forbidden_post.txt" \
    "405" \
    "forbidden_post.txt"

# Test 5: Successful CGI Request (GET)
run_test \
    "Successful CGI Request (GET)" \
    "curl -v http://${SERVER_HOST}:${SERVER_PORT_8080}/cgi-bin/test.php" \
    "200" \
    ""

# Test 6: Successful CGI Request (POST)
run_test \
    "Successful CGI Request (POST)" \
    "curl -v -X POST -d \"name=Gemini\" http://${SERVER_HOST}:${SERVER_PORT_8080}/cgi-bin/test_post.php" \
    "200" \
    ""

# Test 7: Successful File Deletion (DELETE)
run_test \
    "Successful File Deletion" \
    "curl -v -X DELETE http://${SERVER_HOST}:${SERVER_PORT_8080}/delete_me.txt" \
    "200" \
    ""

cleanup_all

echo "\n--- All tests completed ---"
