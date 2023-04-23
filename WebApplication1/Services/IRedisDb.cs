namespace APIServer.Services;
public interface IRedisDb
{
    // 추후 기능 추가 예정
    public Task<ErrorCode> RegistUser(string email, string authToken, Int64 accountid);
}
