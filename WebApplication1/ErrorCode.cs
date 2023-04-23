public enum ErrorCode : UInt16
{
    None = 0,

    CreateAccountFailDuplicate = 1001,
    CreateAccountFailException = 1002,

    LoginFailUserNotExist = 2001,
    LoginFailPwNotMatch = 2002,
    LoginFailAddRedis = 2003,
    VerifyAccountFailException = 2004,
    RegistUserFailException = 2005

}
