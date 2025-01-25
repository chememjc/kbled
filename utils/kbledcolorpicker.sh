#!/bin/bash
clamp() {
  local value=$1
  if ((value < 0)); then
    echo 0
  elif ((value > 255)); then
  echo 255
  else
    echo $value
  fi
}

# Set default values
bred=127
bgrn=127
bblu=127
fred=127
fgrn=63
fblu=63
inc=1
mode="b"

if [ $# -eq 6 ]; then
  bred=$(clamp "$1")
  bgrn=$(clamp "$2")
  bblu=$(clamp "$3")
  fred=$(clamp "$4")
  fgrn=$(clamp "$5")
  fblu=$(clamp "$6")
fi
echo "press 7,4 for red+-, 8,5 for green+- and 9,6 for blue+-; 2-3 to change step size-+"
echo "Press q to exit, b to select background f to select foreground"
echo "startng configurator"
kbledclient -bl $bred $bgrn $bblu -fo $fred $fgrn $fblu
echo -ne "Backlight: [ $bred, $bgrn, $bblu ] Focus: [ $fred, $fgrn, $fblu ] $mode step=$inc           "

while true; do
  # Read a single keypress
  read -n1 -s key

  # Check if Enter key is pressed
  if [[ -z $key ]]; then
    echo -e "\n"
    echo "Backlight: [ $bred, $bgrn, $bblu ]"
    echo "Focus:     [ $fred, $fgrn, $fblu ]"
    echo -e "\n"
  fi

  # Check if q key is pressed
  if [[ $key =~ 'q' ]]; then
    echo -e "\nExiting script."
    break
  fi

  # Check if b key is pressed
  if [[ $key =~ 'b' ]]; then
    mode="b"
    echo -ne "\rBacklight Adjust Mode"
  fi

  # Check if f key is pressed
  if [[ $key =~ 'f' ]]; then
    mode="f"
    echo -ne "\rFocus Adjust Mode"
  fi

  # Check if the key is a number between 4 and 9
  if [[ $key =~ [4-9] ]] & [ $mode = "b" ]; then
    # update the variables
    if [[ $key =~ '7' ]]; then
        bred=$(clamp $(($bred+$inc)))
    elif [[ $key =~ '4' ]]; then
        bred=$(clamp $(($bred-$inc)))
    elif [[ $key =~ '8' ]]; then
        bgrn=$(clamp $(($bgrn+$inc)))
    elif [[ $key =~ '5' ]]; then
        bgrn=$(clamp $(($bgrn-$inc)))
    elif [[ $key =~ '9' ]]; then
        bblu=$(clamp $(($bblu+$inc)))
    elif [[ $key =~ '6' ]]; then
        bblu=$(clamp $(($bblu-$inc)))
    fi
    echo -ne "\rBacklight: [ $bred, $bgrn, $bblu ] Focus: [ $fred, $fgrn, $fblu ] $mode $inc          "
    kbledclient -bl $bred $bgrn $bblu
  elif [[ $key =~ [4-9] ]] & [ $mode = "f" ]; then
    # update the variables
    if [[ $key =~ '7' ]]; then
      fred=$(clamp $(($fred+$inc)))
    elif [[ $key =~ '4' ]]; then
      fred=$(clamp $(($fred-$inc)))
    elif [[ $key =~ '8' ]]; then
      fgrn=$(clamp $(($fgrn+$inc)))
    elif [[ $key =~ '5' ]]; then
      fgrn=$(clamp $(($fgrn-$inc)))
    elif [[ $key =~ '9' ]]; then
      fblu=$(clamp $(($fblu+$inc)))
    elif [[ $key =~ '6' ]]; then
      fblu=$(clamp $(($fblu-$inc)))
    fi
    echo -ne "\rBacklight: [ $bred, $bgrn, $bblu ] Focus: [ $fred, $fgrn, $fblu ] $mode $inc          "
    kbledclient -fo $fred $fgrn $fblu
  fi
  if [[ $key =~ [2-3] ]]; then
    if [[ $key =~ '2' ]]; then
      inc=$(clamp "$(($inc-1))")
    elif [[ $key =~ '3' ]]; then
      inc=$(clamp "$(($inc+1))")
    fi
    if [ $inc -eq "0" ]; then
      inc="1"
    fi
    echo -ne "\rBacklight: [ $bred, $bgrn, $bblu ] Focus: [ $fred, $fgrn, $fblu ] $mode $inc          "
  fi
done
