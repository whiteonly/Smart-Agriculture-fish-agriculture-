-- SmartContainer Database Schema (with min/max tracking)

CREATE TABLE IF NOT EXISTS `sensor_data` (
    `id`            INT AUTO_INCREMENT PRIMARY KEY,
    `device_id`     VARCHAR(32) NOT NULL,
    `temperature`   FLOAT,
    `humidity`      FLOAT,
    `water_level`   INT,
    `water_drop`    TINYINT DEFAULT 0,
    `status`        VARCHAR(20) DEFAULT 'Normal',
    `min_temp`      DECIMAL(5,2) NULL,
    `max_temp`      DECIMAL(5,2) NULL,
    `min_hum`       DECIMAL(5,2) NULL,
    `max_hum`       DECIMAL(5,2) NULL,
    `created_at`    DATETIME DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `device_settings` (
    `id`                    INT PRIMARY KEY,
    `device_id`             VARCHAR(32) NOT NULL,
    `temp_threshold`        FLOAT DEFAULT 31.0,
    `humidity_threshold`    FLOAT DEFAULT 60.0,
    `water_level_threshold` INT DEFAULT 500,
    `upload_interval`       INT DEFAULT 10,
    `buzzer_enabled`        TINYINT DEFAULT 1,
    `servo1_open`           TINYINT DEFAULT 0,
    `servo2_open`           TINYINT DEFAULT 0,
    `updated_at`            DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `device_settings` (`id`, `device_id`)
SELECT 1, 'esp32_01'
WHERE NOT EXISTS (SELECT 1 FROM `device_settings` WHERE `id` = 1);