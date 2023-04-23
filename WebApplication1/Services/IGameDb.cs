namespace APIServer.Services;
public interface IGameDb
{
    // 추후 기능 추가 예정
    public Task<ErrorCode> CreateAccount(string email, string password);
    public Task<Tuple<ErrorCode, Int64>> VerifyAccount(string email, string password);
}
