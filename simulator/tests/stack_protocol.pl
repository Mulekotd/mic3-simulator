use strict;
use warnings;
use IO::Socket::UNIX;
use Socket qw(SOCK_STREAM);

alarm 10;

my $socket = IO::Socket::UNIX->new(
    Type => SOCK_STREAM,
    Peer => '/run/mic3/simulator.sock'
) or die "socket local indisponível: $!\n";

sub exact {
    my ($size) = @_;
    my $data = '';
    while (length($data) < $size) {
        my $received = sysread($socket, my $part, $size - length($data));
        die "EOF inesperado\n" unless $received;
        $data .= $part;
    }
    return $data;
}

sub receiveFrame {
    my ($type, $size) = unpack('Cv', exact(3));
    return ($type, exact($size));
}

sub sendFrame {
    my ($type, $payload) = @_;
    my $data = pack('Cv', $type, length($payload)) . $payload;
    while (length($data)) {
        my $sent = syswrite($socket, $data);
        die "envio falhou\n" unless $sent;
        substr($data, 0, $sent, '');
    }
}

sub ack {
    my ($command) = @_;
    my ($type, $payload) = receiveFrame();
    die "ACK inesperado para $command\n"
        unless $type == 0x81 && $payload eq pack('CC', $command, 0);
}

open my $program, '<:raw', '/data/procedure_call.bin' or die $!;
my $image = do { local $/; <$program> };

sendFrame(0x01, pack('VV', 0, length($image) / 4));
ack(0x01);
sendFrame(0x02, pack('V', 0) . $image);
ack(0x02);
sendFrame(0x03, '');
ack(0x03);

my ($initialType, $initialPayload) = receiveFrame();
die "pilha inicial ausente\n" unless $initialType == 0x97 &&
    (unpack('VVvC', $initialPayload))[2] == 0;

sendFrame(0x12, pack('VCv', 1000000, 2, 1));
ack(0x12);

my ($pushed, $written, $popped, $output, $halted, $stopped, $stats) =
   (0, 0, 0, undef, 0, 0, 0);

while (!$stats) {
    my ($type, $payload) = receiveFrame();
    if ($type == 0x97) {
        my ($cycle, $sp, $depth, $count) = unpack('VVvC', $payload);
        $pushed = 1 if $sp == 0xFFB && $depth == 1;
        $written = 1 if $sp == 0xFFB && $count == 1 &&
            unpack('V', substr($payload, 11, 4)) == 2;
        $popped = 1 if $pushed && $sp == 0xFFC && $depth == 0;
    } elsif ($type == 0x91) {
        $output = unpack('xV', $payload);
    } elsif ($type == 0x93) {
        $halted = 1;
    } elsif ($type == 0x95) {
        $stopped = 1;
    } elsif ($type == 0x94) {
        $stats = 1;
    }
}

die "pilha ou saída divergente\n"
    unless $pushed && $written && $popped && defined($output) &&
           $output == 14 && $halted && $stopped;

for (1..3) {
    sendFrame(0x17, '');
    ack(0x17);
    my ($rewoundType) = receiveFrame();
    my ($cycleType) = receiveFrame();
    my ($stackType, $stackPayload) = receiveFrame();
    my ($statsType) = receiveFrame();
    die "retorno incompleto\n"
        unless $rewoundType == 0x96 && $cycleType == 0x90 &&
               $stackType == 0x97 && $statsType == 0x94;

    if ($_ == 3) {
        my (undef, $sp, $depth, $count) = unpack('VVvC', $stackPayload);
        die "pilha não restaurada\n" unless $sp == 0xFFB && $depth == 1 &&
            $count == 1 && unpack('V', substr($stackPayload, 11, 4)) == 2;
    }
}

print "CALL/RETN: retorno empilhado, removido e restaurado; saída 14\n";
