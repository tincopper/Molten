<?php
$servername = "10.40.6.187";
$username = "root";
$password = "123456";
$dbname = "ejob";

// 创建连接
$conn = new mysqli($servername, $username, $password, $dbname);
// Check connection
if ($conn->connect_error) {
    die("连接失败: " . $conn->connect_error);
}

$sql = "SELECT app_key, app_secret FROM ejob_key_secret";
$result = $conn->query($sql);

if ($result->num_rows > 0) {
    // 输出数据
    while($row = $result->fetch_assoc()) {
        echo "app_key: " . $row["app_key"]. " - app_secret: " . $row["app_secret"]."</br>";
    }
} else {
    echo "0 结果";
}
$conn->close();
?>