<?php
require_once __DIR__ . '/config.php';

header('Access-Control-Allow-Origin: https://socstudentmusicforlife.com/fish_agriculture/php');
header('Access-Control-Allow-Credentials: true');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    header('Access-Control-Allow-Methods: POST, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type');
    http_response_code(204);
    exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    header('Content-Type: application/json');
    http_response_code(405);
    echo json_encode(['error' => 'Method Not Allowed']);
    exit;
}

session_start();
require_auth();

header('Content-Type: application/json');

$input = json_decode(file_get_contents('php://input'), true);
if (!$input) {
    http_response_code(400);
    echo json_encode(['error' => 'Invalid JSON']);
    exit;
}

try {
    $db = get_db();
    $stmt = $db->prepare("UPDATE device_settings SET
        temp_threshold        = ?,
        humidity_threshold    = ?,
        water_level_threshold = ?,
        upload_interval       = ?,
        buzzer_enabled        = ?,
        servo1_open           = ?,
        servo2_open           = ?,
        updated_at            = CURRENT_TIMESTAMP
        WHERE device_id = ?");
    $stmt->execute([
        $input['temp_threshold']        ?? 31.0,
        $input['humidity_threshold']    ?? 70.0,
        $input['water_level_threshold'] ?? 26.5,
        $input['upload_interval']       ?? 10,
        $input['buzzer_enabled']        ?? 1,
        $input['servo1_open']           ?? 0,
        $input['servo2_open']           ?? 0,
        $input['device_id']             ?? 'esp32_01',
    ]);
    echo json_encode(['success' => true]);
} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode(['error' => $e->getMessage()]);
}