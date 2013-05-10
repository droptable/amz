#!/usr/bin/php
<?php

// gen html_table

$RDIR = __DIR__ . '/res';

`php $RDIR/htmlent_table.php`;

// compile

$CC = 'gcc';
$CFLAGS = '-Wall -std=c11 -g -D_GNU_SOURCE -O2';
$LDFLAGS = '-lm -lpcre -lcurl';

$SDIR = __DIR__ . '/src';
$ODIR = __DIR__ . '/bin';

$OBJS = '';

foreach (glob($SDIR . '/*.c') as $c) {
  $OBJ = $ODIR . '/' . basename($c, '.c') . '.o';
  $OBJS .= " $OBJ";
  `$CC -c -o $OBJ $c $CFLAGS`;
}

`$CC -o $ODIR/amz $OBJS $CFLAGS $LDFLAGS`;
`rm -f $ODIR/*.o`;
  


