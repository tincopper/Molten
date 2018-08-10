<?php
$servername = "10.40.6.187";
$username = "root";
$password = "123456";

// 创建连接
$conn = mysqli_connect($servername, $username, $password);

echo '<br/>';
// 检测连接
if (!$conn) {
    die("Connection failed: " . mysqli_connect_error());
}
echo "连接成功";
?>