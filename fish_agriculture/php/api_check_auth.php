<?php
require_once __DIR__ . '/config.php';

session_start();   // once, before any output

header('Access-Control-Allow-Origin: https://socstudentmusicforlife.com/fish_agriculture');
header('Access-Control-Allow-Credentials: true');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    header('Access-Control-Allow-Methods: GET, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type');
    http_response_code(204);
    exit;
}

header('Content-Type: application/json');

if (!empty($_SESSION['authenticated']) && $_SESSION['authenticated'] === true) {
    echo json_encode(['authenticated' => true]);
} else {
    http_response_code(401);
    echo json_encode(['authenticated' => false]);
}