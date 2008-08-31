#!/usr/bin/perl -W

# $Id$

# Copyright (C) 2008  The Xastir Group
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.


# Look at the README for more information on the program.
#
# Run it like this:
#
#   cd xastir/config
#   ../scripts/langPirateEnglish.pl -split <language-English.sys >language-PirateEnglish.sys
# or
#   ../scripts/langPirateEnglish.pl <some-input-file >some-output-file
#
# "-split": Translate 2nd part of line only (Xastir language file).
# Without it:  Translate entire text.


# Regex strings derived from:
# http://userscripts.org/scripts/review/25998
# http://dougal.gunters.org/blog/2004/08/30/text-filter-suite


my @regexs = (

  # Custom for Xastir:
  "\\bham\\b:matey",
  "\\bhi hi\\b:it be a joke, matey",
  "\\bk7gps\\b:Th\' Good Cap\'n", 
  "\\bDave Dobbins\\b:Th\' Good Cap\'n", 
  "\\bAPRS\'er\\b:Fellow Pirate",  
  "\\b[Aa]prs\'er\\b:fellow pirate", 
  "\\bHerb Gerhardt\\b:scurvy dog",
  "\\bkb7uvc\\b:scurvy dog",
  "\\bwa7nwp\\b:cabin boy",
  "\\b[Mm]ap\\b:Treasure Map",
  "\\bXastir\\b:HMS Xastir",
  "\\bxastir\\b:HMS xastir",
  "\\bXASTIR\\b:HMS XASTIR",
  "\\bStation:Ship",
  "\\bstation:ship",
  "\\bView:Gander",
  "\\bview:gander",
  "\\bFile:Scroll",
  "\\bfile:scroll",
#  "\\bFile:Parchment",
#  "\\bfile:parchment",
  "\\bMessage:Dispatch",
  "\\bmessage:dispatch",
  "\\bLogging:Scribblin'",
  "\\blogging:scribblin'",
  "\\bLog:Scribble",
  "\\blog:scribble",
  "\\bPrint:Affix to parchment",
  "\\bprint:affix to parchment",
  "\\bCancel:Nay",
  "\\bcancel:nay",
  "Close:Nay",
  "close:nay",
  "\\bOK:Aye",
  "\\bOk:Aye",
  "\\bok:aye",
  "\\bQuit:Nay",
  "\\bquit:nay",
  "\\bExit:Run Away!",
  "\\bexit:run away!",
  "\\bSelect:Choose 'yer Weapon!",
  "\\bselect:choose 'yer weapon!",
  "\\bCompressed:Scrawny",
  "\\bcompressed:scrawny",
  "\\bAudio:Racket",
  "\\baudio:racket",
  "\\bFont:Scribble",
  "\\bfont:scribble",
  "\\bSatellite:Heavenly Body",
  "\\bsatellite:heavenly body",
  "\\bSnapshot:Etching",
  "\\bsnapshot:etching",
#  "\\bObject:",
#  "\\bobject:",
#  "\\bItem:",
#  "\\bitem:",
  "\\bInterface:Grapple",
  "\\binterface:grapple",
  "\\bConfigure:Provision me' ship",
  "\\bconfigure:provision me' ship",

  # From userscripts:
  "About:\'bout",
  "\\babout\\b:\'bout",
  "\\ba lot\\b:mightily",
  "\\bam\\b:be",
  "\\bamputee\\b:peg leg",
  "\\bafraid\\b:lily-livered",
  "\\band\\b:an\'",
  "\\baround\\b:\'round",
  "\\battack\\b:pillage",
  "\\battacked\\b:raped and pillaged",
  "\\barrest\\b:keelhaul",
  "\\bAIDS\\b:scurvy",
  "\\baids\\b:scurvy",
  "\\bATTN\\b:AVAST",

  "\\bbad\\b:scurvy",
  "\\bbeer\\b:grog",
  "\\bvodka\\b:grog",
  "\\bban him\\b:make him walk the plank",
  "\\bcar\\b:ship",
  "\\bBAN HIM\\b:Make him walk the plank!",
  "\\bBan him\\b:Make him walk the plank",

  "\\bale\\b:grog",
  "\\bbetween\\b:betwixt",
  "\\bwhiskey\\b:grog",
  "\\bbeauty\\b:gov\'nor\'s daughter",
  "\\bbefore\\b:\'ere",
  "\\bbanned\\b:forced t\' walk the plank",
  "\\bbetween\\b:\'tween",
  "\\bboy\\b:jim lad",
  "\\bboys\\b:jim lads",
  "\\bbought\\b:pilfered",
  "\\b4chan\\b:House o\' Bilge Rats",

  "\\bAsia\\b:Th\' Mystic East",
  "\\bJapan\\b:Th\' Mystic East",
  "\\bChina\\b:Th\' Mystic East",
  "\\bKorea\\b:Th\' Mystic East",
  "\\basia\\b:Th\' Mystic East",
  "\\bjapan\\b:Th\' Mystic East",
  "\\bchina\\b:Th\' Mystic East",
  "\\bkorea\\b:Th\' Mystic East",
  "\\bIndia\\b:Hindustan",
  "\\bIsrael\\b:Th\' Holy Lands",
  "\\bindia\\b:Hindustan",
  "\\bisrael\\b:Th\' Holy Lands",
  "\\bIraq\\b:Th\' Ottoman Empire",
  "\\bIran\\b:Th\' Ottoman Empire",
  "\\bPakistan\\b:Th\' Ottoman Empire",
  "\\bAfghanistan\\b:Th\' Ottoman Empire",
  "\\biraq\\b:Th\' Ottoman Empire",
  "\\biran\\b:Th\' Ottoman Empire",
  "\\bpakistan\\b:Th\' Ottoman Empire",
  "\\bafghanistan\\b:Th\' Ottoman Empire",
  "\\bAfrica\\b:Th\' Dark Continent",
  "\\bafrica\\b:Th\' Dark Continent",

  "\\bcheat\\b:hornswaggle",
  "\\bchild\\b:wee one",
  "\\bchildren\\b:wee ones",
  "\\bcoffee\\b:grog",
  "\\bcondemn\\b:keelhaul",

  "\\bconference\\b:parlay",
  "\\bcrazy\\b:addled",
  "\\bjapanophile\\b:scurvy mutt",
  "\\bweeaboo\\b:scurvy mutt",
  "\\boh crap\\b:shiver me timbers!",
  "\\bover\\b:o\'er", 
  "\\bThe Token Shop\\b:Honest Jack\'s Swag Shop", 
  "\\bToken Shop\\b:Honest Jack\'s Swag Shop", 

  "\\bdamn\\b:damn\'ed",    
  "\\bdevil\\b:Davy Jones",    
  "\\bdie\\b:head to Davy Jones\' Locker",  
  "\\bdead\\b:\'n Davy Jones\' Locker",
  "\\bdoesn\'t\\b:don\'t",
  "\\bdollars\\b:pieces o\' eight",
  "\\beveryone\\b:all hands",
  "\\beyewear\\b:eye patch",
  "\\bglasses\\b:eye patches",
  "\\bfight\\b:duel",
  "\\bgreatly\\b:mightily",
  "\\bgold\\b:dubloons",
  "\\bha\\b:har har",
  "\\bhaha\\b:har har",
  "\\bbase\\b:port",
  "\\bfort\\b:port",
  "\\bhah\\b:har har",
  "\\bheh\\b:har har",
  "\\bHa\\b:Har har",
  "\\bflag\\b:Jolly Roger",
  "\\bhouse\\b:shanty",
  "\\bidiot\\b:bilge rat",

  "\\bhit\\b:flog",
  "\\btorrents\\b:Blackbeard\'s treasure ",
  "\\btorrent\\b:Blackbeard\'s treasure",

  "\\bn00b\\b:landlubber", 
  "\\bnoob\\b:landlubber", 
  "\\btroll\\b:blowhard",
  "\\bdrive\\b:sail",
  "\\bcoins\\b:pieces o\' eight",
  "\\bcorrect\\b:right an\' true",

  "\\bfly\\b:sail",
  "\\bfool\\b:squiffy",
  "\\bfoolish\\b:addled",
  "\\bfor\\b:fer",
  "\\bFor\\b:Fer",
  "\\bfriend\\b:matey",
  "\\bfriends\\b:hearties",
  "\\bgirl\\b:lass",
  "\\bex-girlfriend\\b:festerin\' harlot",
  "\\bex girlfriend\\b:festerin\' harlot",
  "\\bgood\\b:worthy",
  "\\byou\'re\\b:yer",  
  "\\byour\\b:yer", 

  "\\bhello\\b:ahoy",
  "\\bHello\\b:Ahoy",
  "\\bhey\\b:avast!",
  "\\bHey\\b:Avast",
  "\\bhey\\b:avast!",
  "\\bhi\\b:ahoy", 
  "\\bHi\\b:Ahoy", 
  "\\bHiya\\b:Ahoy",   
  "\\bhiya\\b:ahoy",   
  "\\bmoney\\b:booty", 
  "\\bguy\\b:feller",  
  "\\bfellow\\b:feller",
  "\\bidiot\\b:scalawag",
  "ing\\b:in\'",
  "\\bin\\b:\'n",
  "\\bis\\b:be",
  "\\bit\'s\\b:\'tis",
  "\\bit is\\b:\'tis",
  "\\bkid\\b:wee one",
  "\\bkids\\b:wee ones",
  "\\bkill\\b:keelhaul",

  "\\bis not\\b:be not",
  "\\baren\'t\\b:be not",
  "\\bare\\b:be",
  "\\bam\\b:be",
  "\\bAre\\b:Be",
  "\\blol\\b:yo ho ho!",
  "\\blolol\\b:Me sides be splittin\'!",
  "\\bodd\\b:addled",
  "\\bof\\b:o\'",
  "\\bohmigod\\b:begad!",
  "\\bomigod\\b:begad!",
  "\\bomg\\b:begad!",
  "\\bOMG\\b:BEGAD!",
  "\\bo rly\\b:be that right, sailor?",
  "\\borly\\b:be that right, sailor?",
  "\\bya rly\\b:Sailor, \'tis true",
  "\\byarly\\b:Sailor, \'tis true",
  "\\bwhoamg\\b:shiver me timbers!",

  "\\bmoney\\b:booty",
  "\\bmy\\b:me",
  "\\bprosecute\\b:keelhaul",
  "\\bpants\\b:britches",
  "\\bHello\\b:Ahoy!", 
  "\\bquick\\b:smart",
  "\\bquickly\\b:smartly",
  "\\bthe rules\\b:the Pirate\'s Code",
  "\\bnice\\b:fine",
  "\\bthe Internet\\b:The Seven Seas",
  "\\bThe Internet\\b:The Seven Seas",
  "\\binternet\\b:Seven Seas",
  "\\bInternet\\b:Seven Seas",

  "\\bsilly\\b:addled",
  "\\bsword\\b:cutlass",
  "\\bshe\\b:the lass",
  "\\bshut up\\b:pipe down",
#  "\\bspeech:parlance",
#  "\\bSpeech:Parlance",
  "\\bspeech:parley",
  "\\bSpeech:Parley",
  "\\bsteal\\b:commandeer",
  "\\bdownload\\b:plunder",
  "\\bDownload\\b:Plunder",

  "\\bsexy\\b:saucy",
  "\\btelescope\\b:spyglass",
  "\\bterrorist\\b:scourge o\' the seven seas",
  "\\bterrorists\\b:scalawags",
  "tion\\b:tin\'",
  "\\bthere\\b:thar",
  "tions\\b:tin\'s",
  "\\bto\\b:t\'",
  "\\btomorrow\\b:the morrow",
  "\\btruck\\b:vessel",
    
  "\\bwasn\'t\\b:weren\'t",
  "\\bwant to\\b:wish t\'",
  "\\bwanna\\b:wish t\'",
  "\\bYep\\b:Aye", 
  "\\byep\\b:aye", 
  "\\bwoman\\b:buxom beauty",
  "\\bwomen\\b:wenches",
  "\\bwin\\b:triumph",
  "\\bwins\\b:triumphs",
  "\\bwork\\b:deck swabbing",
  "\\bwine\\b:grog",
  "\\byes\\b:aye",
  "\\bYes\\b:Aye",
  "\\bno\\b:nay",
  "\\bNo\\b:Nay",
  "\\bnah\\b:nay",
  "\\bNah\\b:Nay",
  "\\bYeah\\b:Aye",
  "\\byeah\\b:Aye",

  "\\byou\\b:ye",
  "\\byour\\b:yer",
  "\\bwtf\\b:what devilry!",
  "\\bWTF\\b:Begad, what devilry be is?!",
  "\\bFacebook\\b:PirateBook",
  "\\bThis message was deleted at the request of the original poster\\b:This here message be taken back by a yellow-bellied pirate",
  "\\bThis message has been deleted by a moderator\\b:This poor soul had a run-in with the authorities",
  "\\bGood Tokens\\b:Pieces o\' Eight",
  "\\bBad Tokens\\b:Black Marks",
  "\\byou\'re\\b:yer",
  "\\bYou\'re\\b:Yer",
  "\\bwins\\b:triumphs",   

  "\\bHome|/ \\b:Haven",
  "\\bAdd a link|/ \\b:Add plunder",
  "\\bRandom link|/ \\b:Who needs maps?",
  "\\bTop rated links|/ \\b:Quality grog",
  "\\bLinks o\' the week|/ \\b:Modern fashions",
  "\\bWiki|/ \\b:Wikis",
  "\\bAll links|/ \\b:All th\' plunder",
  "\\bFavorites|/ \\b:Treasures",
  "\\bSearch|/ \\b:Scour",
  "\\bStats|/ \\b:Specs",
  "\\bBoards|/ \\b:Th\' Tavern",
  "\\bUser List|/ \\b:Roster",
  "\\bLogout|/ \\b:Retreat",
  "\\bHelp|/ \\b:Aid",
  "\\bBoard List|/ \\b:Port",
  "\\bCreate New Topic|/ \\b:Parley",
  "\\bPost New Message|/ \\b:Parley",
  "\\bNext Page|/ \\b:Next Map",
  "\\bTagged|/ \\b:X\'d",
  "\\bTag|/ \\b:X",

  # From text-filter-suite:
  "\\bmy\\b:me",
  "\\bboss\\b:admiral",
  "\\bmanager\\b:admiral",
  "\\b[Cc]aptain\\b:Cap\'n",
  "\\bmyself\\b:meself",
  "\\byour\\b:yer",
  "\\byou\\b:ye",
  "\\bfriend\\b:matey",
  "\\bfriends\\b:maties",
  "\\bco[-]?worker\\b:shipmate",
  "\\bco[-]?workers\\b:shipmates",
  "\\bpeople\\b:scallywags",
  "\\bearlier\\b:afore",
  "\\bold\\b:auld",
  "\\bthe\\b:th\'",
  "\\bof\\b:o'",
  "\\bdon\'t\\b:dern\'t",
  "\\bdo not\\b:dern\'t",
  "\\bnever\\b:ne\'er",
  "\\bever\\b:e\'er",
  "\\bover\\b:o\'er",
  "\\bYes\\b:Aye",
  "\\bNo\\b:Nay",
  "\\bYeah\\b:Aye",
  "\\byeah\\b:aye",
  "\\bdon\'t know\\b:dinna",
  "\\bdidn\'t know\\b:did nay know",
  "\\bhadn\'t\\b:ha\'nae",
  "\\bdidn\'t\\b:di\'nae",
  "\\bwasn\'t\\b:weren\'t",
  "\\bhaven\'t\\b:ha\'nae",
  "\\bfor\\b:fer",
  "\\bbetween\\b:betwixt",
  "\\baround\\b:aroun\'",
  "\\bto\\b:t\'",
  "\\bit\'s\\b:\'tis",
  "\\bwoman\\b:wench",
  "\\bwomen\\b:wenches",
  "\\blady\\b:wench",
  "\\bwife\\b:lady",
  "\\bgirl\\b:lass",
  "\\bgirls\\b:lassies",
  "\\bguy\\b:lubber",
  "\\bman\\b:lubber",
  "\\bfellow\\b:lubber",
  "\\bdude\\b:lubber",
  "\\bboy\\b:lad",
  "\\bboys\\b:laddies",
  "\\bchildren\\b:little sandcrabs",
  "\\bkids\\b:minnows",
  "\\bhim\\b:that scurvey dog",
  "\\bher\\b:that comely wench",
  "\\bhim\.\\b:that drunken sailor",
  "\\bHe\\b:The ornery cuss",
  "\\bShe\\b:The winsome lass",
  "\\bhe\'s\\b:he be",
  "\\bshe\'s\\b:she be",
  "\\bwas\\b:were bein\'",
  "\\bHey\\b:Avast",
  "\\bher\.\\b:that lovely lass",
  "\\bfood\\b:chow",
  "\\bmoney\\b:dubloons",
  "\\bdollars\\b:pieces of eight",
  "\\bcents\\b:shillings",
  "\\broad\\b:sea",
  "\\broads\\b:seas",
  "\\bstreet\\b:river",
  "\\bstreets\\b:rivers",
  "\\bhighway\\b:ocean",
  "\\bhighways\\b:oceans",
  "\\binterstate\\b:high sea",
  "\\bprobably\\b:likely",
  "\\bidea\\b:notion",
  "\\bcar\\b:boat",
  "\\bcars\\b:boats",
  "\\btruck\\b:schooner",
  "\\btrucks\\b:schooners",
  "\\bSUV\\b:ship",
  "\\bairplane\\b:flying machine",
  "\\bjet\\b:flying machine",
  "\\bmachine\\b:contraption",
  "\\bdriving\\b:sailing",
  "\\bunderstand\\b:reckon",
  "\\bdrive\\b:sail",
  "\\bdied\\b:snuffed it",
  "ing\\b/:in\'",
  "ings\\b/:in\'s",
#
# These next two do cool random substitutions
#  "(\.\s)/e:avast("$0",3)",
#  "([!\?]\s)/e:avast("$0",2)" # Greater chance after exclamation
);


# Randomize use of this array, both in order and in frequency of
# use.  Not currently used at all.
#my @shouts = (
#  ", avast$stub",
#  "$stub Ahoy!",
#  ", and a bottle of rum!",
#  ", by Blackbeard's sword$stub",
#  ", by Davy Jones' locker$stub",
#  "$stub Walk the plank!",
#  "$stub Aarrr!",
#  "$stub Yaaarrrrr!",
#  ", pass the grog!",
#  ", and dinna spare the whip!",
#  ", with a chest full of booty$stub",
#  ", and a bucket o' chum$stub",
#  ", we'll keel-haul ye!",
#  "$stub Shiver me timbers!",
#  "$stub And hoist the mainsail!",
#  "$stub And swab the deck!",
#  ", ye scurvey dog$stub",
#  "$stub Fire the cannons!",
#  ", to be sure$stub",
#  ", I'll warrant ye$stub",
#  ", on a dead man's chest!",
#  "$stub Load the cannons!",
#  "$stub Prepare to be boarded!",
#  ", I'll warrant ye$stub",
#  "$stub Ye'll be sleepin' with the fishes!",
#  "$stub The sharks will eat well tonight!",
#  "$stub Oho!",
#  "$stub Fetch me spyglass!",
# );

# Randomize use of this array both in order and frequency of use.
# Not currently used at all.
my @openings = (
  'Avast! ',
  'Yarrr! ',
  'Blimey! ',
  'Ahoy! ',
  'Harrr! ',
  'Aye aye! ',
  'Shiver me timbers! ',
  'Arrrr! '
);


# Check whether we're translating an Xastir language file or plain
# text:
#   "-split" present:  Translate the 2nd piece of each line.
#   "-split" absent:   Translate the entire text.
my $a;
if ($#ARGV < 0) { $a = ""; }
else            { $a = shift; }
$do_split = 0;
if (length($a) > 0 && $a =~ m/-split/) {
  $do_split = 1;
}

while ( <> ) {

  # Change the "Id:" RCS tag to show that we translated the file.
  if (m/^#.*\$Id:/) {
      print "# language-PirateEnglish.sys, translated from language-English.sys\n";
      print "# Please do not edit this derived file.\n";
      next;
  }
  # Skip other comment lines
  if (m/^#/) {
    next;
  }

  if ($do_split) {
    # Split each incoming line by the '|' character
    @pieces = split /\|/;
  }

  foreach my $test (@regexs) {

      @reg_parts = split /\:/, $test;

      if ($do_split) {
        # Translate the second portion of each line only
        $pieces[1] =~ s/$reg_parts[0]/$reg_parts[1]/g;
      }
      else {
        # Translate the entire line of text
        s/$reg_parts[0]/$reg_parts[1]/g;
      }
  }

  # Add an opening phrase to each line randomly?

  if ($do_split) {
    # Combine the line again for output to STDOUT
    print join '|', @pieces;
  }
  else {
    print;
  }
}


