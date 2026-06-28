<?php
require_once __DIR__ . '/config.php';

session_start();
header('Access-Control-Allow-Origin: https://socstudentmusicforlife.com/fish_agriculture');
header('Access-Control-Allow-Credentials: true');
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    header('Access-Control-Allow-Methods: POST, GET, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type');
    http_response_code(204);
    exit;
}
require_auth();
header('Content-Type: application/json');
try {
    $db = get_db();
    $device = $_GET['device_id'] ?? 'esp32_01';
    $minutes = (int)($_GET['minutes'] ?? 30);  // default: last 30 minutes
    // First try: get data from last N minutes (real-time)
    $stmt = $db->prepare("
        SELECT * FROM sensor_data 
        WHERE device_id = ? 
          AND created_at >= DATE_SUB(NOW(), INTERVAL {$minutes} MINUTE)
        ORDER BY id ASC
    ");
    $stmt->execute([$device]);
    $data = $stmt->fetchAll();
    // Fallback: if no recent data, get last 30 records
    if (empty($data)) {
        $stmt = $db->prepare("
            SELECT * FROM sensor_data 
            WHERE device_id = ? 
            ORDER BY id DESC 
            LIMIT 30
        ");
        $stmt->execute([$device]);
        $data = array_reverse($stmt->fetchAll());
    }
    echo json_encode($data);
} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode(['error' => $e->getMessage()]);
}