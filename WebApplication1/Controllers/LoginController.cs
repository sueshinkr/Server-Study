using APIServer.Services;
using Microsoft.AspNetCore.Mvc;
using Microsoft.OpenApi.Extensions;
using Org.BouncyCastle.Utilities;
using System.Security.Cryptography;
using ZLogger;

namespace APIServer.Controllers;

[ApiController]
[Route("[controller]")]
public class Login : ControllerBase
{
    readonly ILogger<Login> _logger;
    readonly IGameDb _gameDb;
    readonly IRedisDb _redisDb;

    public Login(ILogger<Login> logger, IGameDb gamedb, IRedisDb redisdb)
    {
        _logger = logger;
        _gameDb = gamedb;
        _redisDb = redisdb;
    }

    [HttpPost]
    public async Task<PkLoginResponse> Post(PkLoginRequest request)
    {
        var response = new PkLoginResponse();
        response.Result = ErrorCode.None;

        var (errorCode, accountId) = await _gameDb.VerifyAccount(request.Email, request.Password);
        if (errorCode != ErrorCode.None)
        {
            response.Result = errorCode;
            return response;
        }

        var authToken = CreateAuthToken();
        errorCode = await _redisDb.RegistUser(request.Email, authToken, accountId);
        if (errorCode != ErrorCode.None)
        {
            response.Result = errorCode;
            return response;
        }

        _logger.ZLogInformation($"{request.Email} Login Success");

        response.Authtoken = authToken;
        return response;
    }

    public string CreateAuthToken()
    {
        const string AllowableCharacters = "abcdefghijklmnopqrstuvwxyz0123456789";

        var bytes = new Byte[25];
        using (var random = RandomNumberGenerator.Create())
        {
            random.GetBytes(bytes);
        }
        return new string(bytes.Select(x => AllowableCharacters[x % AllowableCharacters.Length]).ToArray());
    }
}

public class PkLoginRequest
{
    public string? Email { get; set; }
    public string? Password { get; set; }
}

public class PkLoginResponse
{
    public ErrorCode Result { get; set; }
    public string? Authtoken { get; set; }
}