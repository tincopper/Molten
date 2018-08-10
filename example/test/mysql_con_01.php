<?php
$servername = "10.40.6.187";
$username = "root";
$password = "123456";

/*
 * php输入毫秒部分的代码
 * */
function msectime() {
	list($msec, $sec) = explode(' ', microtime());
	$msectime =  (float)sprintf('%.0f', (floatval($msec) + floatval($sec)) * 1000);
	return $msectime;
}

$t1 = msectime();

// 创建连接
$conn = mysqli_connect($servername, $username, $password);

$t2 = msectime();
//两次时间减一下
$t = $t2 - $t1;

echo '<br/>';
print_r($t);

echo '<br/>';
// 检测连接
if (!$conn) {
    die("Connection failed: " . mysqli_connect_error());
}
echo "连接成功";
?>