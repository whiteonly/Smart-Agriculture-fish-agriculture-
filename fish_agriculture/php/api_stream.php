<?php
require_once __DIR__ . '/config.php';
header('Content-Type: text/event-stream');
header('Cache-Control: no-cache');
header('Connection: keep-alive');
header('Access-Control-Allow-Origin: https://socstudentmusicforlife.com/fish_agriculture');
header('Access-Control-Allow-Credentials: true');
//CRITICAL: disable all PHP output buffering
//Server-Sent Events (SSE) real time 1 second change
if (function_exists('apache_setenv')) @apache_setenv('no-gzip', '1');
@ini_set('zlib.output_compression', '0');
@ini_set('output_buffering', '0');
@ini_set('implicit_flush', '1');
while (ob_get_level()) ob_end_flush();
ob_implicit_flush(true);
set_time_limit(0);

$db = get_db();
$lastId = 0;
while (true) {
    try {// get all data 
        $stmt = $db->query("SELECT * FROM sensor_data ORDER BY id DESC LIMIT 1");
        $row = $stmt->fetch();

        if ($row && $row['id'] != $lastId) {
            $lastId = $row['id'];
            echo "data: " . json_encode($row) . "\n\n";
            flush();          //push immediately to browser
        }
    } catch (PDOException $e) {
        echo "data: " . json_encode(['error' => $e->getMessage()]) . "\n\n";
        flush();
    }
    // Keep-alive comment so the browser doesn't drop the connection
    echo ": ping\n\n";
    flush();
    sleep(1);   // polls database every 1 second
}