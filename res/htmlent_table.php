#!/usr/bin/php
<?php

$ip = fopen(__DIR__ . '/htmlent_table.txt', 'r');
$op = fopen(__DIR__ . '/htmlent_table.gen', 'w+');

const RE = '/(&(?:#\d+|\w+);)\s+-->\s((?:\s|.*?))(?:\s+|\n)/';

while (false !== $ln = fgets($ip)) {
  if (!preg_match_all(RE , $ln, $m))
    continue;
  
  foreach ($m[1] as $k => $v)
    fwrite($op, '  { "' . nq($v) . '", "' . nq($m[2][$k]) . '"},' . "\n");
}

fclose($ip);
fclose($op);

function nq($s) {
  return strtr($s, [ '"' => '\\"' ]);
}
