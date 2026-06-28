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
require_auth();
header('Content-Type: application/json');
try {
    $db = get_db();
    $stmt = $db->query("SELECT * FROM sensor_data ORDER BY id DESC LIMIT 1");
    $data = $stmt->fetch();
    echo json_encode($data ?: ['error' => 'No data yet']);
} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode(['error' => $e->getMessage()]);
}

