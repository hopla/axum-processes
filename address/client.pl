#!/usr/bin/perl

# Requirements:
#   Standard Perl install
#   JSON::XS       http://search.cpan.org/dist/JSON-XS/
#   POE            http://search.cpan.org/dist/POE/
#   Term::ReadKey  http://search.cpan.org/dist/TermReadKey/

package AC;

my $unix_path = "/tmp/axum-address";

use strict;
use warnings;
use JSON::XS;
use POSIX 'strftime';
use Socket 'AF_UNIX';
use POE;
use POE::Wheel::SocketFactory;
use POE::Wheel::ReadWrite;
use POE::Wheel::ReadLine;


POE::Session->create(package_states => [
  AC => [qw|_start cli_input sock_connect sock_fail sock_input sock_error _stop|]
]);
$poe_kernel->run();
exit;


sub _start {
  # open terminal
  $_[HEAP]{rl} = POE::Wheel::ReadLine->new(InputEvent => 'cli_input');
  $_[HEAP]{rl}->get("addr> ");

  # try to connect
  $_[HEAP]{sf} = POE::Wheel::SocketFactory->new(
    SocketDomain => AF_UNIX,
    RemoteAddress => $unix_path,
    SuccessEvent => 'sock_connect',
    FailureEvent => 'sock_fail',
  );

  # settings
  $_[HEAP]{raw} = 0;
}


sub _stop {
  delete $_[HEAP]{rl};
  delete $_[HEAP]{rw};
  delete $_[HEAP]{sf};
}


sub cli_input {
  $_[KERNEL]->yield('_stop') if !defined $_[ARG0];
  $_[HEAP]{rl}->addhistory($_[ARG0]);
  local $_ = $_[ARG0];
  return $_[KERNEL]->yield('_stop') if /^(?:q|quit|exit|bye)$/;

  # raw
  if(/^raw$/) {
    $_[HEAP]{raw} = !$_[HEAP]{raw};
    $_[HEAP]{rl}->put($_[HEAP]{raw} ? 'Raw mode on.' : 'Raw mode off.');

  # connect
  } elsif(/^con(?:nect)? (.+)$/) {
    if(defined $_[HEAP]{rw}) {
      $_[HEAP]{rl}->put('ALready connected.');
    }
    $_[HEAP]{sf} = POE::Wheel::SocketFactory->new(
      SocketDomain => AF_UNIX,
      RemoteAddress => $1,
      SuccessEvent => 'sock_connect',
      FailureEvent => 'sock_fail',
    );

  # help
  } elsif(/^help$/) {
    $_[HEAP]{rl}->put($_) for(
      'raw                 Toggle display of raw replies',
      'con path            Connect to a UNIX socket',
      'dis                 Disconnect',
      'send str            Send a raw string to the server',
      'get [expr]          Get a list of nodes matching expr',
      'setname addr name   Set the name of a node',
      'setengine addr eng  Set the default engine address of a node',
      'refresh [expr]      Re-fetch information of nodes matchin expr',
      'ping                Broadcast ping',
      'remove addr         Remove reservation for addr',
      'reasign old new     Reassign a node a new address',
      'notify [+-]flag     Enable or disable notifications for flag'
    );

  # all following commands require a connection
  } elsif(!defined $_[HEAP]{rw}) {
    $_[HEAP]{rl}->put('Not connected.');

  # disconnect
  } elsif(/^dis(connect)?$/) {
    delete $_[HEAP]{rw};
    delete $_[HEAP]{sf};
    $_[HEAP]{rl}->put('Connection closed.');

  # send a raw command
  } elsif(/^send (.+)$/) {
    $_[HEAP]{rw}->put($1);

  # GET
  } elsif(/^(?:get|list)( .+)?$/) {
    $_[HEAP]{rw}->put('GET '.encode_json($1 ? {limit => 50, eval $1} : {limit => 50}));

  # SETNAME
  } elsif(/^setname ([0-9a-fA-F]{8}) (.+)$/) {
    $_[HEAP]{rw}->put('SETNAME '.encode_json({MambaNetAddr => $1, Name => $2}));

  # SETENGINE
  } elsif(/^setengine ([0-9a-fA-F]{8}) ([0-9a-fA-F]{8})$/) {
    $_[HEAP]{rw}->put('SETENGINE '.encode_json({MambaNetAddr => $1, EngineAddr => $2}));

  # REFRESH
  } elsif(/^refresh( .+)?$/) {
    $_[HEAP]{rw}->put('REFRESH '.encode_json($1 ? {eval $1} : {}));

  # PING
  } elsif(/^ping$/) {
    $_[HEAP]{rw}->put('PING {}');

  # REMOVE
  } elsif(/^remove ([0-9a-fA-F]{8})$/) {
    $_[HEAP]{rw}->put('REMOVE '.encode_json({MambaNetAddr => $1}));

  # REASSIGN
  } elsif(/^reassign ([0-9a-fA-F]{8}) ([0-9a-fA-F]{8})$/) {
    $_[HEAP]{rw}->put('REASSIGN '.encode_json({old => $1, new => $2}));

  # NOTIFY
  } elsif(/^notify ([+-])(.+)?$/) {
    $_[HEAP]{rw}->put('NOTIFY '.encode_json({ $1 eq '+' ? 'enable' : 'disable', [uc $2]}));

  # unkown command
  } else {
    $_[HEAP]{rl}->put('Unkown command.');
  }

  $_[HEAP]{rl}->get('addr> ');
}


sub sock_input {
  return $_[HEAP]{rl}->put($_[ARG0]) if $_[HEAP]{raw};
  local $_ = $_[ARG0];
  if(/^ERROR (.+)$/) {
    $_[HEAP]{rl}->put("Error: ".decode_json($1)->{msg});

  } elsif(/^OK (.+)$/) {
    $_[HEAP]{rl}->put("OK");

  } elsif(/^RELEASED (.+)$/) {
    $_[HEAP]{rl}->put("MambaNet Address released: ".decode_json($1)->{MambaNetAddr});

  } elsif(/^NODES (.+)$/) {
    my $re = decode_json($1);
    return $_[HEAP]{rl}->put('No result.')
      if !$re || !$re->{result} || !@{$re->{result}};
    $_[HEAP]{rl}->put(' Address    UniqueID        Parent          S   Engine    FirstSeen      LastSeen       #AR  Name');
    for my $i (@{$re->{result}}) {
      $i->{$_} = !$i->{$_} ? '-' : strftime '%Y%m%d %H%M', gmtime $i->{$_} for (qw|FirstSeen LastSeen|);
      $_[HEAP]{rl}->put(sprintf ' %s%s  %s  %s  %02X  %s  %-13s  %-13s  %3d  %s', $i->{Active} ? '*' : ' ',
        @{$i}{qw| MambaNetAddr UniqueID Parent Services EngineAddr FirstSeen LastSeen AddressRequests Name|});
    }

  } else {
    $_[HEAP]{rl}->put("Unkown message: $_")
  }
}


sub sock_connect {
  $_[HEAP]{rl}->put("Connected to $_[ARG1]");
  $_[HEAP]{rw} = POE::Wheel::ReadWrite->new(
    Handle => $_[ARG0],
    InputEvent => 'sock_input',
    ErrorEvent => 'sock_error'
  );
}


sub sock_fail {
  $_[HEAP]{rl}->put("Couldn't connect: $_[ARG2]");
  delete $_[HEAP]{sf};
  delete $_[HEAP]{rw};
}


sub sock_error {
  $_[HEAP]{rl}->put($_[ARG1] ? "$_[ARG0] failed: $_[ARG2]" : "Server disconnected.");
  delete $_[HEAP]{sf};
  delete $_[HEAP]{rw};
}

