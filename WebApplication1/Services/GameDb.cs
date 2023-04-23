using System.Data;
using APIServer.Controllers;
using APIServer.ModelDB;
//using Microsoft.Extensions.Options;
using MySqlConnector;
using SqlKata.Execution;
using ZLogger;
//using static Org.BouncyCastle.Math.EC.ECCurve;

namespace APIServer.Services;

public class GameDb : IGameDb
{
    readonly ILogger<GameDb> _logger;

    IDbConnection _dbConn;
    QueryFactory _queryFactory;

    public GameDb(ILogger<GameDb> logger, IConfiguration configuration)
    {
        _logger = logger;

        var GameDBConnectString = configuration.GetSection("DBConnection")["GameDB"];
        _dbConn = new MySqlConnection(GameDBConnectString);

        var compiler = new SqlKata.Compilers.MySqlCompiler();
        _queryFactory = new SqlKata.Execution.QueryFactory(_dbConn, compiler);

        _logger.ZLogInformation("MySQL Db Connected");
    }

    public async Task<ErrorCode> CreateAccount(string email, string password)
    {
        try
        {
            var count = await _queryFactory.Query("account").InsertAsync(new
            {
                Email = email,
                Password = password
            });

            if (count != 1)
            {
                return ErrorCode.CreateAccountFailDuplicate;
            }
            return ErrorCode.None;
        }
        catch (Exception ex)
        {
            _logger.ZLogError(ex, $"[CreateAccount] ErrorCode: {ErrorCode.CreateAccountFailException}, Email: {email}");
            return ErrorCode.CreateAccountFailException;
        }
    }

    public async Task<Tuple<ErrorCode, Int64>> VerifyAccount(string email, string password)
    {
        try
        {
            var accountinfo = await _queryFactory.Query("account").Where("Email", email).FirstOrDefaultAsync<Account>();
            if (accountinfo is null || accountinfo.AccountId == 0)
            {
                return new Tuple<ErrorCode, Int64>(ErrorCode.LoginFailUserNotExist, 0);
            }

            if (accountinfo.Password != password)
            {
                _logger.ZLogError($"[VerifyAccount] ErrorCode: {ErrorCode.LoginFailPwNotMatch}, Email: {email}");
                return new Tuple<ErrorCode, Int64>(ErrorCode.LoginFailPwNotMatch, 0);
            }

            return new Tuple<ErrorCode, Int64>(ErrorCode.None, accountinfo.AccountId);
        }
        catch (Exception ex)
        {
            _logger.ZLogError(ex, $"[VerifyAccount] ErrorCode: {ErrorCode.VerifyAccountFailException}, Email: {email}");
            return new Tuple<ErrorCode, Int64>(ErrorCode.VerifyAccountFailException, 0);
        }
    }

}
