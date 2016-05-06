# abc-cli bash-completion (Airbitz command line client - airbitz.co)

_abc_cli_complete()
{
    local cur_word prev_word type_list

    # COMP_WORDS is an array of words in the current command line.
    # COMP_CWORD is the index of the current word (the one the cursor is
    # in). So COMP_WORDS[COMP_CWORD] is the current word; we also record
    # the previous word here, although this specific script doesn't
    # use it yet.
    cur_word="${COMP_WORDS[COMP_CWORD]}"
    prev_word="${COMP_WORDS[COMP_CWORD-1]}"

    # Ask abc-cli to generate a list of types it supports
    type_list='account-available
    account-create
    account-decrypt
    account-encrypt
    account-list
    account-sync
    address-allocate
    address-calculate
    address-list
    address-search
    bitid-login
    bitid-sign
    category-add
    category-list
    category-remove
    change-password
    change-password-recovery
    check-password
    check-recovery-answers
    exchange-fetch
    exchange-update
    exchange-validate
    general-update
    get-question-choices
    get-questions
    hiddenbits-generate
    otp-auth-get
    otp-auth-remove
    otp-auth-set
    otp-key-get
    otp-key-remove
    otp-key-set
    otp-reset-get
    otp-reset-remove
    pin-login
    pin-login-setup
    plugin-clear
    plugin-get
    plugin-keys
    plugin-list
    plugin-remove
    plugin-set
    repo-clone
    repo-sync
    settings-get
    settings-set-nickname
    settings-set-recovery-reminder
    sign-in
    spend-address
    spend-airbitz-fee
    spend-bip70
    spend-get-fee
    spend-get-max
    spend-transfer
    stratum-version
    upload-logs
    version
    wallet-archive
    wallet-create
    wallet-decrypt
    wallet-encrypt
    wallet-info
    wallet-list
    wallet-order
    wallet-remove
    wallet-seed
    wallet-sync
    watcher'

    # COMPREPLY is the array of possible completions, generated with
    # the compgen builtin.
    if [[ ${prev_word} == 'abc-cli' ]] ; then
        COMPREPLY=( $(compgen -W "${type_list}" -- ${cur_word}) )
    else
        COMPREPLY=()
    fi
    return 0
}

# Register _pss_complete to provide completion for the following commands
complete -F _abc_cli_complete abc-cli
