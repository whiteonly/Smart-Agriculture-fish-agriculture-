<?php
require_once __DIR__ . '/config.php';
header('Access-Control-Allow-Origin: https://socstudentmusicforlife.com/fish_agriculture');
header('Access-Control-Allow-Credentials: true');
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    header('Access-Control-Allow-Methods: POST, GET, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type');
    http_response_code(204);
    exit;
}
session_start();
header('Content-Type: application/json');
$device = $_GET['device_id'] ?? 'esp32_01';
try {
    $db = get_db();
    $stmt = $db->prepare("SELECT * FROM device_settings WHERE device_id = ?");
    $stmt->execute([$device]);
    $settings = $stmt->fetch();
    echo json_encode($settings ?: ['error' => 'Device not found']);
} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode(['error' => $e->getMessage()]);
}

