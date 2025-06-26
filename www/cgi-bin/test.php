#!/usr/bin/php
<?php
echo "Status: 200 OK\r\n";
echo "Content-Type: text/plain\r\n";
echo "\r\n";
echo "Hello from PHP CGI!\n";
echo "Method: " . $_SERVER['REQUEST_METHOD'] . "\n";
echo "Query String: " . $_SERVER['QUERY_STRING'] . "\n";
echo "Body: " . file_get_contents('php://stdin') . "\n";
?>