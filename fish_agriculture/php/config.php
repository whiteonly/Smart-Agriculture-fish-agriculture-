<?php
/**
 * MySQL + Auth Configuration
 */

define('DB_HOST', 'localhost');
define('DB_NAME', 'musicbvk_fish_agriculture');    //  database
define('DB_USER', 'musicbvk_azri'); //user change
define('DB_PASS', 'lme?;3KZm9Vv'); // authority
define('APP_FOLDER', '/fish_agriculture');

define('AUTH_USER', 'admin');
define('AUTH_PASS_HASH', '$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2.uheWG/igi'); // = "password"

function get_db() {
    static $pdo = null;
    if ($pdo === null) {
        $dsn = sprintf('mysql:host=%s;dbname=%s;charset=utf8mb4', DB_HOST, DB_NAME);
        $pdo = new PDO($dsn, DB_USER, DB_PASS);
        $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
        $pdo->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
        $pdo->setAttribute(PDO::ATTR_EMULATE_PREPARES, true);
    }
    return $pdo;
}

function require_auth() {
    if (empty($_SESSION['authenticated']) || $_SESSION['authenticated'] !== true) {
        http_response_code(401);
        header('Content-Type: application/json');
        echo json_encode(['error' => 'Unauthorized']);
        exit;
    }
}
