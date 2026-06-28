<?php
require_once __DIR__ . '/config.php';
header('Access-Control-Allow-Origin: https://socstudentmusicforlife.com/fish_agriculture');
header('Access-Control-Allow-Credentials: true');
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    header('Access-Control-Allow-Methods: POST, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type');
    http_response_code(204);
    exit;
}
session_start();
header('Content-Type: application/json');
$input = json_decode(file_get_contents('php://input'), true);
if (!$input || empty($input['username']) || empty($input['password'])) {
    http_response_code(400);
    echo json_encode(['error' => 'Missing username or password']);
    exit;
}
if ($input['username'] === AUTH_USER && password_verify($input['password'], AUTH_PASS_HASH)) {
    $_SESSION['authenticated'] = true;
    $_SESSION['login_time']      = time();
    echo json_encode(['success' => true]);
} else {
    http_response_code(401);
    echo json_encode(['error' => 'Invalid credentials']);
}
