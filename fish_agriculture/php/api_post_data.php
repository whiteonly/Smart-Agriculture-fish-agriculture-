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

$input = json_decode(file_get_contents('php://input'), true);
if (!$input || !isset($input['device_id'])) {
    http_response_code(400);
    echo json_encode(['error' => 'Missing or invalid JSON body']);
    exit;
}

try {
    $db = get_db();
    $stmt = $db->prepare("INSERT INTO sensor_data
        (device_id, temperature, humidity, water_level, water_drop, status, min_temp, max_temp, min_hum, max_hum)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    $stmt->execute([
        $input['device_id'],
        $input['temperature']  ?? null,
        $input['humidity']     ?? null,
        $input['water_level']  ?? null,
        $input['water_drop']   ?? 0,
        $input['status']       ?? 'Normal',
        $input['min_temp']     ?? null,
        $input['max_temp']     ?? null,
        $input['min_hum']      ?? null,
        $input['max_hum']      ?? null,
    ]);
    echo json_encode(['success' => true, 'id' => $db->lastInsertId()]);
} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode(['error' => $e->getMessage()]);
}