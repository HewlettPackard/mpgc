#!/usr/bin/perl -w

$n = 0;

while(<>) {
    chop;
    my ($type, $addr) = split m| / |;
    $ln = sprintf("%7d", $n++);
    print "$addr $ln:  $type\n";
}

